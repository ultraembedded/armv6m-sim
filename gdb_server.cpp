#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>

#include "gdb_server.h"

/* Based on MSPDebug - debugging tool for MSP430 MCUs
 *          Copyright (C) 2009, 2010 Daniel Beer
 */

/*
arm-none-eabi-gdb program.elf
target remote localhost:3333
cont
*/

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define MAX_MEM_XFER    8192

#define DEBUG_LEVEL     0
#define DPRINTF(l,a)    do { if ((l) <= DEBUG_LEVEL) printf a; } while (0)

#define GDB_NUM_REG   16

//-----------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------
gdb_server::gdb_server(Armv6m *cpu)
{
    m_cpu = cpu;

    m_rx_head = 0;
    m_rx_tail = 0;
    m_tx_len  = 0;
    m_error   = 0;
}
//-----------------------------------------------------------------
// gdb_printf
//-----------------------------------------------------------------
void gdb_server::gdb_printf(const char *fmt, ...)
{
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(m_tx_buf + m_tx_len,
            sizeof(m_tx_buf) - m_tx_len,
            fmt, ap);
    va_end(ap);

#if DEBUG_LEVEL > 10
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
#endif

    m_tx_len += len;
}
//-----------------------------------------------------------------
// gdb_read
//-----------------------------------------------------------------
int gdb_server::gdb_read(bool blocking)
{
    fd_set r;
    int len;
    struct timeval to = { 0, 0 };

    FD_ZERO(&r);
    FD_SET(m_socket, &r);

    if (select(m_socket + 1, &r, NULL, NULL, blocking ? NULL : &to) < 0)
    {
        printf("gdb: select\n");
        return -1;
    }    

    if (!FD_ISSET(m_socket, &r))
        return 0;

    len = recv(m_socket, m_rx_buf, sizeof(m_rx_buf), 0);

    if (len < 0)
    {
        m_error = errno;
        printf("gdb: recv");
        return -1;
    }

    if (!len)
    {
        DPRINTF(1, ("Connection closed\n"));
        return -1;
    }

    DPRINTF(11, ("GDB: Received %d bytes\n", len));

    m_rx_head = 0;
    m_rx_tail = len;
    return len;
}
//-----------------------------------------------------------------
// gdb_peek
//-----------------------------------------------------------------
int gdb_server::gdb_peek(void)
{
    if (m_rx_head == m_rx_tail && gdb_read(false) < 0)
        return -1;

    return m_rx_head != m_rx_tail;
}
//-----------------------------------------------------------------
// gdb_getc
//-----------------------------------------------------------------
int gdb_server::gdb_getc(void)
{
    /* If the buffer is empty, receive some more data */
    if (m_rx_head == m_rx_tail && gdb_read(true) < 0)
        return -1;

    return m_rx_buf[m_rx_head++];
}
//-----------------------------------------------------------------
// gdb_flush
//-----------------------------------------------------------------
int gdb_server::gdb_flush(void)
{
    if (send(m_socket, m_tx_buf, m_tx_len, 0) < 0)
    {
        m_error = errno;
        printf("gdb: flush");
        return -1;
    }

    m_tx_len = 0;
    return 0;
}
//-----------------------------------------------------------------
// gdb_flush_ack
//-----------------------------------------------------------------
int gdb_server::gdb_flush_ack(void)
{
    int c;

    do
    {
        m_tx_buf[m_tx_len] = 0;
#ifdef DEBUG_GDB
        DPRINTF(11, ("-> %s\n", m_tx_buf));
#endif
        if (send(m_socket, m_tx_buf, m_tx_len, 0) < 0)
        {
            m_error = errno;
            printf("gdb: flush_ack");
            return -1;
        }

        c = gdb_getc();
        if (c < 0)
            return -1;
    }
    while (c != '+');

    m_tx_len = 0;
    return 0;
}
//-----------------------------------------------------------------
// gdb_packet_start
//-----------------------------------------------------------------
void gdb_server::gdb_packet_start(void)
{
    gdb_printf("$");
}
//-----------------------------------------------------------------
// gdb_packet_end
//-----------------------------------------------------------------
void gdb_server::gdb_packet_end(void)
{
    int i;
    int c = 0;

    for (i = 1; i < m_tx_len; i++)
        c = (c + m_tx_buf[i]) & 0xff;
    gdb_printf("#%02x", c);
}
//-----------------------------------------------------------------
// gdb_send
//-----------------------------------------------------------------
int gdb_server::gdb_send(const char *msg)
{
    gdb_packet_start();
    gdb_printf("%s", msg);
    gdb_packet_end();
    return gdb_flush_ack();
}
//-----------------------------------------------------------------
// gdb_is_alpha_char:
//-----------------------------------------------------------------
static int gdb_is_alpha_char(char c)
{
    if((c >= '0') && (c <= '9'))
        return 1;
    else if((c >= 'A') && (c <= 'F'))
        return 1;
    else if((c >= 'a') && (c <= 'f'))
        return 1;
    else
        return 0;
}
//-----------------------------------------------------------------
// gdb_hexchar_to_num:
//-----------------------------------------------------------------
static int gdb_hexchar_to_num(int c)
{
    if ((c >= '0') && (c <= '9'))
        return c - '0';
    if ((c >= 'A') && (c <= 'F'))
        return c - 'A' + 10;
    if ((c >= 'a') && (c <= 'f'))
        return c - 'a' + 10;

    return 0;
}
//-----------------------------------------------------------------
// atoui:
//-----------------------------------------------------------------
uint32_t gdb_server::atoui(const char *str)
{
    uint32_t sum = 0;
    uint32_t leftshift = 0;
    char *s = (char*)str;
    char c;
    int length = 0;
    
    // Skip leading spaces
    while (*s && *s == ' ')
        s++;

    // Leading '0x'
    if (*s && *s == '0' && *(s+1) == 'x')
        s+=2;

    str = s;
            
    // Find the end
    while (*s && gdb_is_alpha_char(*s) && length < 8)
    {
        s++;
        length++;
    }

    // Backwards through the string
    while(s != str)
    {
        s--;
        c = *s;

        if((c >= '0') && (c <= '9'))
            sum += (c-'0') << leftshift;
        else if((c >= 'A') && (c <= 'F'))
            sum += (c-'A'+10) << leftshift;
        else if((c >= 'a') && (c <= 'f'))
            sum += (c-'a'+10) << leftshift;
        else
            break;
        
        leftshift+=4;
    }

    return sum;
}
//-----------------------------------------------------------------
// read_registers:
//-----------------------------------------------------------------
int gdb_server::read_registers(void)
{
    uint32_t regs[GDB_NUM_REG];
    int i;
    int err = 0;

    DPRINTF(5, ("Reading registers\n"));

    for (i=0;i<GDB_NUM_REG;i++)
        regs[i] = m_cpu->get_register(i);

    if (err)
        return gdb_send("E00");

    gdb_packet_start();

    for (i = 0; i < GDB_NUM_REG; i++)
        gdb_printf("%02x%02x%02x%02x", (regs[i] >> 0) & 0xFF, (regs[i] >> 8) & 0xFF, (regs[i] >> 16) & 0xFF, (regs[i] >> 24) & 0xFF);

    gdb_packet_end();
    return gdb_flush_ack();
}
//-----------------------------------------------------------------
// read_register:
//-----------------------------------------------------------------
int gdb_server::read_register(char *buf)
{
    uint32_t reg_num = atoui(buf);
    uint32_t reg_val;
    DPRINTF(5, ("Reading register: %d (0x%x)\n", reg_num, reg_num));

    reg_val = m_cpu->get_register(reg_num);

    gdb_packet_start();
    gdb_printf("%02x%02x%02x%02x", (reg_val >> 0) & 0xFF, (reg_val >> 8) & 0xFF, (reg_val >> 16) & 0xFF, (reg_val >> 24) & 0xFF);
    gdb_packet_end();
    return gdb_flush_ack();
}
//-----------------------------------------------------------------
// write_registers:
//-----------------------------------------------------------------
int gdb_server::write_registers(char *buf)
{
    uint32_t val;
    int i;
    int count = strlen(buf) / 4;
    int err = 0;

    if (count <= 0)
        return gdb_send("OK");

    DPRINTF(5, ("Writing registers\n"));
    for (i = 0; i < count; i++)
    {
        val = atoui(buf);

        m_cpu->set_register(i, val);

        buf += 8;
    }

    if (err)
        return gdb_send("E00");

    return gdb_send("OK");
}
//-----------------------------------------------------------------
// read_memory:
//-----------------------------------------------------------------
int gdb_server::read_memory(char *text)
{
    char *length_text = strchr(text, ',');
    uint32_t length, addr;
    unsigned char buf[MAX_MEM_XFER];
    int i;

    if (!length_text)
    {
        printf("gdb: malformed memory read request\n");
        return gdb_send("E00");
    }

    *(length_text++) = 0;

    length = (uint32_t)atoui(length_text);
    addr = (uint32_t)atoui(text);

    if (length > sizeof(buf))
        length = sizeof(buf);

    DPRINTF(5, ("Reading %3d bytes from 0x%04x\n", length, addr));

    for (uint32_t i=0;i<length;i++)
        buf[i] = m_cpu->read(addr + i);

    gdb_packet_start();
    for (i = 0; i < length; i++)
        gdb_printf("%02x", buf[i]);
    gdb_packet_end();

    return gdb_flush_ack();
}
//-----------------------------------------------------------------
// write_memory:
//-----------------------------------------------------------------
int gdb_server::write_memory(char *text)
{
    char *data_text   = strchr(text, ':');
    char *length_text = strchr(text, ',');
    uint32_t length, addr;
    unsigned char buf[MAX_MEM_XFER];
    int buflen = 0;

    if (!(data_text && length_text))
    {
        printf("gdb: malformed memory write request\n");
        return gdb_send("E00");
    }

    *(data_text++) = 0;
    *(length_text++) = 0;

    length = (uint32_t)atoui(length_text);
    addr = (uint32_t)atoui(text);

    while (buflen < sizeof(buf) && *data_text && data_text[1])
    {
        buf[buflen++] = (gdb_hexchar_to_num(data_text[0]) << 4) |
                         gdb_hexchar_to_num(data_text[1]);
        data_text += 2;
    }

    if (buflen != length)
    {
        printf("gdb: length mismatch\n");
        return gdb_send("E00");
    }

    DPRINTF(5, ("Writing %3d bytes to 0x%04x\n", length, addr));

    for (uint32_t i=0;i<buflen;i++)
        m_cpu->write(addr + i, buf[i]);

    return gdb_send("OK");
}
//-----------------------------------------------------------------
// set_pc:
//-----------------------------------------------------------------
int gdb_server::set_pc(char *buf)
{
    uint32_t val;

    if (!buf || !*buf)
        return 0;

    val = (uint32_t)atoui(buf);

    DPRINTF(5, ("Set PC to 0x%x\n", val));
    m_cpu->set_pc(htonl(val));
    return 0;
}
//-----------------------------------------------------------------
// send_status:
//-----------------------------------------------------------------
int gdb_server::send_status(int status)
{
    return gdb_send("S05");
}
//-----------------------------------------------------------------
// run:
//-----------------------------------------------------------------
int gdb_server::run(char *buf, int instructions)
{
    uint32_t val = 0;
    DPRINTF(5, ("Running\n"));

    if (set_pc(buf))
        return gdb_send("E00");

    while (instructions == -1 || (--instructions >= 0))
    {
        if (m_cpu->get_break())
        {
            DPRINTF(5, ("Break detected\n"));
            break;
        }

        m_cpu->step();

        // GDB Interrupt
        if (gdb_read(false) != 0)
            break;
    }

    DPRINTF(5, ("Stopping\n"));
    return send_status(5);
}
//-----------------------------------------------------------------
// single_step:
//-----------------------------------------------------------------
int gdb_server::single_step(char *buf)
{
    DPRINTF(5, ("Single stepping\n"));
    return run(buf, 1);
}
//-----------------------------------------------------------------
// set_breakpoint:
//-----------------------------------------------------------------
int gdb_server::set_breakpoint(int enable, char *buf)
{
    char *parts[3];
    int type;
    uint32_t addr;
    int i;
    
    // Split string into upto 3 parts
    
    // [Breakpoint type]
    parts[0] = buf;
    
    // [Address]
    parts[1] = strchr(buf, ',');
    if (parts[1])
    {
        // Null terminate previous string & skip character
        *parts[1] = '\0';
        parts[1]++;
    }
    
    // [Width]
    parts[2] = strchr(parts[1], ',');
    if (parts[2])
    {
        // Null terminate previous string & skip character
        *parts[2] = '\0';
        parts[2]++;
    }
    
    /* Make sure there's a type argument */
    if (!parts[0])
    {
        printf("gdb: breakpoint requested with no type\n");
        return gdb_send("E00");
    }

    /* We only support breakpoints (not data watch points) */
    if (*parts[0] != '0' && *parts[0] != '1')
    {
        printf("gdb: unsupported breakpoint type: %s\n",
            parts[0]);
        return gdb_send("");
    }

    /* There needs to be an address specified */
    if (!parts[1])
    {
        printf("gdb: breakpoint address missing\n");
        return gdb_send("E00");
    }

    /* Parse the breakpoint address */
    addr = (uint32_t)atoui(parts[1]);

    if (enable) 
    {
        if (!m_cpu->set_breakpoint(addr))
            return gdb_send("E00");
    }
    else
    {
        if (!m_cpu->clr_breakpoint(addr))
            return gdb_send("E00");
    }

    return gdb_send("OK");
}
//-----------------------------------------------------------------
// gdb_send_supported:
//-----------------------------------------------------------------
int gdb_server::gdb_send_supported(void)
{
    gdb_packet_start();
    gdb_printf("PacketSize=%x", MAX_MEM_XFER * 2);
    gdb_packet_end();
    return gdb_flush_ack();
}
//-----------------------------------------------------------------
// gdb_send_offsets:
//-----------------------------------------------------------------
int gdb_server::gdb_send_offsets(void)
{
    gdb_packet_start();
    gdb_printf("Text=0;Data=0;Bss=0"); // TODO
    gdb_packet_end();
    return gdb_flush_ack();
}
//-----------------------------------------------------------------
// process_gdb_command:
//-----------------------------------------------------------------
int gdb_server::process_gdb_command(char *buf, int len)
{
    DPRINTF(6, ("\n\nGDB Command: %c\n", buf[0]));

    switch (buf[0]) {
    case '?': /* Return target halt reason */
        return send_status(5);

    case 'z':
    case 'Z':
        DPRINTF(4, ("GDB: %s breakpoint\n", (buf[0] == 'Z') ? "Set" : "Clear" ));
        return set_breakpoint(buf[0] == 'Z', buf + 1);

    case 'g':
        DPRINTF(4, ("GDB: Read Registers\n"));
        return read_registers();

    case 'G':
        DPRINTF(4, ("GDB: Write Registers\n"));
        return write_registers(buf + 1);

    case 'p':
        DPRINTF(4, ("GDB: Read Registers\n"));
        return read_register(buf + 1);

    case 'q': /* Query */
        if (!strncmp(buf, "qRcmd,", 6))
            return gdb_send("E00");
        if (!strncmp(buf, "qSupported", 10))
            return gdb_send_supported();
        if (!strncmp(buf, "qOffsets", 10))
            return gdb_send_offsets();
        break;

    case 'm': /* Read memory */
        DPRINTF(4, ("GDB: Read Memory\n"));
        return read_memory(buf + 1);

    case 'M': /* Write memory */
        DPRINTF(4, ("GDB: Write Memory\n"));
        return write_memory(buf + 1);

    case 'c': /* Continue */
        DPRINTF(4, ("GDB: Continue\n"));
        return run(buf + 1,-1);

    case 's': /* Single step */
        DPRINTF(4, ("GDB: Single step\n"));
        return single_step(buf + 1);

    case 'H':
      /* Set the thread number of subsequent operations. For now ignore
         silently and just reply "OK" */
        DPRINTF(4, ("GDB: Thread control %s\n", buf));
        return gdb_send("OK");
    }

    /* For unknown/unsupported packets, return an empty reply */
    DPRINTF(1, ("GDB: Unknown command %s\n", buf));
    return gdb_send("");
}
//-----------------------------------------------------------------
// gdb_server:
//-----------------------------------------------------------------
int gdb_server::start(int port)
{
    int sock;
    int client;
    struct sockaddr_in addr;
    socklen_t len;
    int arg;
    int i;

    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
    {
        printf("gdb: can't create socket");
        return -1;
    }

    arg = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) < 0)
        printf("gdb: warning: can't reuse socket address");

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("gdb: can't bind to port %d: %s\n",
            port, strerror(errno));
        close(sock);
        return -1;
    }

    if (listen(sock, 1) < 0)
    {
        printf("gdb: can't listen on socket");
        close(sock);
        return -1;
    }

    DPRINTF(1, ("Bound to port %d. Now waiting for connection...\n", port));

    len = sizeof(addr);
    client = accept(sock, (struct sockaddr *)&addr, &len);
    if (client < 0)
    {
        printf("gdb: failed to accept connection");
        close(sock);
        return -1;
    }

    close(sock);
    DPRINTF(-1, ("Client connected\n"));

    m_socket = client;

    /* Put the hardware breakpoint setting into a known state. */
    DPRINTF(1, ("Clearing all breakpoints...\n"));
    //breakpoint_init();

    for (;;)
    {
        char buf[MAX_MEM_XFER * 2 + 64];
        int len = 0;
        int cksum_calc = 0;
        int cksum_recv = 0;
        int c;

        /* Wait for packet start */
        do
        {
            c = gdb_getc();
            if (c < 0)
                return -1;
        }
        while (c != '$');

        /* Read packet payload */
        while (len + 1 < sizeof(buf))
        {
            c = gdb_getc();
            if (c < 0)
                return -1;
            if (c == '#')
                break;

            buf[len++] = c;
            cksum_calc = (cksum_calc + c) & 0xff;
        }
        buf[len] = 0;

        DPRINTF(11, ("GDB RX: %s\n", buf));

        /* Read packet checksum */
        c = gdb_getc();
        if (c < 0)
            return -1;
        cksum_recv = gdb_hexchar_to_num(c);
        c = gdb_getc();
        if (c < 0)
            return -1;
        cksum_recv = (cksum_recv << 4) | gdb_hexchar_to_num(c);

        DPRINTF(11, ("<- $%s#%02x\n", buf, cksum_recv));

        if (cksum_recv != cksum_calc)
        {
            printf("gdb: bad checksum (calc = 0x%02x, "
                "recv = 0x%02x)\n", cksum_calc, cksum_recv);
            printf("gdb: packet data was: %s\n", buf);
            gdb_printf("-");
            if (gdb_flush() < 0)
                return -1;
            continue;
        }

        /* Send acknowledgement */
        gdb_printf("+");
        if (gdb_flush() < 0)
            return -1;

        if (len && process_gdb_command(buf, len) < 0)
            return -1;
    }

    return m_error ? -1 : 0;
}