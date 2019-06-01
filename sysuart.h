#ifndef __SYSUART_H__
#define __SYSUART_H__

#include "memory.h"

//-----------------------------------------------------------------
// SysUART: UART device
//-----------------------------------------------------------------
class Sysuart: public Device
{
public:
    Sysuart(uint32_t base_addr, int irq_num)
    {
        reset();
    }
    
    void reset(void)
    {
    }

    void write_reg(uint32_t address, uint32_t data)
    {
        printf("%c",(data >> 0) & 0xFF);
        fflush(stdout);
    }
    uint32_t read_reg(uint32_t address)
    {
        return 0;
    }

    int clock(void)
    {
        return -1;
    }
};

#endif
