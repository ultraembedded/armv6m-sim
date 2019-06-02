#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>

#include "armv6m.h"
#include "elf_load.h"
#include "gdb_server.h"

//-----------------------------------------------------------------
// mem_create: Create memory region
//-----------------------------------------------------------------
static int mem_create(void *arg, uint32_t base, uint32_t size)
{
    Armv6m *sim = (Armv6m *)arg;
    return sim->create_memory(base, size);
}
//-----------------------------------------------------------------
// mem_load: Load byte into memory
//-----------------------------------------------------------------
static int mem_load(void *arg, uint32_t addr, uint8_t data)
{
    Armv6m *sim = (Armv6m *)arg;
    sim->write(addr, data);
    return sim->valid_addr(addr);
}
//-----------------------------------------------------------------
// bin_load: Binary load
//-----------------------------------------------------------------
static int bin_load(const char *filename, cb_mem_create fn_create, cb_mem_load fn_load, void *arg, uint32_t mem_base, uint32_t mem_size, uint32_t *p_start_addr)
{
    // Load file
    FILE *f = fopen(filename, "rb");
    if (f)
    {
        long size;
        char *buf;
        int error = 1;

        // Get size
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        rewind(f);

        buf = (char*)malloc(size+1);
        if (buf)
        {
            // Read file data in
            int len = fread(buf, 1, size, f);
            buf[len] = 0;

            if (!mem_create(arg, mem_base, mem_size))
                fprintf (stderr,"Error: Could not allocate memory\n");
            else
            {
                error = 0;
                for (int i=0;i<len;i++)
                {
                    if (!mem_load(arg, mem_base + i, buf[i]))
                    {
                        fprintf (stderr,"Error: Could not load image to memory\n");
                        error = 1;
                        break;
                    }
                }
            }

            free(buf);
            fclose(f);
        }

        if (p_start_addr)
            *p_start_addr = mem_base;

        return !error;
    }
    else
    {
        fprintf (stderr,"Error: Could not open %s\n", filename);
        return 0;
    }
}
//-----------------------------------------------------------------
// main
//-----------------------------------------------------------------
int main(int argc, char *argv[])
{
    unsigned _cycles = 0;
    int max_cycles = -1;
    char *filename = NULL;
    int help = 0;
    int trace = 0;
    uint32_t trace_mask = ~0;
    uint32_t stop_pc = 0xFFFFFFFF;
    uint32_t trace_pc = 0xFFFFFFFF;
    uint32_t mem_base = 0x20000000;
    uint32_t mem_size = (64 * 1024 * 1024);
    bool explicit_start = false;
    uint32_t explicit_start_addr = 0;
    bool explicit_mem = true;
    bool gdb = false;
    int  gdb_port = 3333;
    int c;

    while ((c = getopt (argc, argv, "t:v:f:c:r:d:b:s:e:X:g")) != -1)
    {
        switch(c)
        {
            case 't':
                trace = strtoul(optarg, NULL, 0);
                break;
            case 'v':
                trace_mask = strtoul(optarg, NULL, 0);
                break;
            case 'r':
                stop_pc = strtoul(optarg, NULL, 0);
                break;
            case 'f':
                filename = optarg;
                break;
            case 'c':
                max_cycles = (int)strtoul(optarg, NULL, 0);
                break;
            case 'b':
                mem_base = strtoul(optarg, NULL, 0);
                explicit_mem = true;
                break;
            case 's':
                mem_size = strtoul(optarg, NULL, 0);
                explicit_mem = true;
                break;
            case 'e':
                trace_pc = strtoul(optarg, NULL, 0);
                break;
            case 'X':
                explicit_start_addr = strtoul(optarg, NULL, 0);
                explicit_start = true;
                break;
            case 'g':
                gdb = true;
                break;
            case '?':
            default:
                help = 1;   
                break;
        }
    }

    if (help || (filename == NULL))
    {
        fprintf (stderr,"Usage:\n");
        fprintf (stderr,"-f filename.[bin/elf] = Executable to load (binary or ELF)\n");
        fprintf (stderr,"-t 1/0                = Enable program trace (1)\n");
        fprintf (stderr,"-v 0xX                = Trace Mask\n");
        fprintf (stderr,"-c nnnn               = Max instructions to execute\n");
        fprintf (stderr,"-r 0xnnnn             = Stop at PC address\n");
        fprintf (stderr,"-e 0xnnnn             = Trace from PC address\n");
        fprintf (stderr,"-b 0xnnnn             = Memory base address (for binary loads)\n");
        fprintf (stderr,"-s nnnn               = Memory size (for binary loads)\n");
        fprintf (stderr,"-X 0xnnnn             = Override start address\n");
        fprintf (stderr,"-g                    = Start GDB server on port 3333\n");
        exit(-1);
    }

    Armv6m *sim = new Armv6m();

    if (explicit_mem)
    {
        printf("MEM: Create memory 0x%08x-%08x\n", mem_base, mem_base + mem_size-1);
        mem_create(sim, mem_base, mem_size);
    }

    uint32_t start_addr = 0;

    char *ext = filename ? strrchr(filename, '.') : NULL;

    // Load ELF file
    if ((ext && !strcmp(ext, ".bin") && bin_load(filename, mem_create, mem_load, NULL, mem_base, mem_size, &start_addr)) ||
        elf_load(filename, mem_create, mem_load, sim, &start_addr))
    {
        // User specified start address
        if (explicit_start)
            start_addr = explicit_start_addr;
        // Find boot vectors if ELF file
        else if (!(ext && !strcmp(ext, ".bin")))
            start_addr = elf_get_symbol(filename, "vectors");

        printf("Starting from 0x%08x\n", start_addr);

        // Reset CPU to given start PC
        sim->reset(start_addr);

        // Enable trace?
        if (trace)
            sim->enable_trace(trace_mask);

        _cycles = 0;

        // GDB server
        if (gdb)
        {
            gdb_server *srv = new gdb_server(sim);
            srv->start(gdb_port);
        }
        // Standalone
        else
        {
            uint32_t current_pc = 0;
            while (!sim->get_fault() && !sim->get_stopped() && current_pc != stop_pc)
            {
                current_pc = sim->get_pc();
                sim->step();
                _cycles++;

                if (max_cycles != -1 && max_cycles == _cycles)
                    break;

                // Turn trace on
                if (trace_pc == current_pc)
                    sim->enable_trace(trace_mask);
            }
        }
    }
    else
        fprintf (stderr,"Error: Could not open %s\n", filename);

    // Fault occurred?
    if (sim->get_fault())
        return 1;
    else
        return 0;
}
