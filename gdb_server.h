#ifndef __GDB_SERVER_H__
#define __GDB_SERVER_H__

#include "armv6m.h"

//-----------------------------------------------------------------
// GDB Server
//-----------------------------------------------------------------
class gdb_server
{
public:
    gdb_server(Armv6m *cpu);

    int start(int port);

    static const int MAX_MEM_XFER = 8192;

protected:
    void gdb_printf(const char *fmt, ...);
    int  gdb_read(bool blocking);
    int  gdb_peek(void);
    int  gdb_getc(void);
    int  gdb_flush(void);
    int  gdb_flush_ack(void);

    void gdb_packet_start(void);
    void gdb_packet_end(void);

    int  gdb_send(const char *msg);

    uint32_t atoui(const char *str);

    int read_registers(void);
    int write_registers(char *buf);
    int read_memory(char *text);
    int write_memory(char *text);
    int set_pc(char *buf);
    int read_register(char *buf);

    int send_status(int status);
    int run(char *buf, int instructions);
    int single_step(char *buf);

    int set_breakpoint(int enable, char *buf);

    int gdb_send_supported(void);
    int gdb_send_offsets(void);

    int process_gdb_command(char *buf, int len);

protected:
    Armv6m        *m_cpu;

    int            m_socket;
    int            m_error;

    char           m_rx_buf[MAX_MEM_XFER];
    int            m_rx_head;
    int            m_rx_tail;

    char           m_tx_buf[MAX_MEM_XFER * 2 + 64];
    int            m_tx_len;
};

#endif
