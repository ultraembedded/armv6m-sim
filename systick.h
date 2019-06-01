#ifndef __SYSTICK_H__
#define __SYSTICK_H__

#include "memory.h"

//-----------------------------------------------------------------
// Defines
//-----------------------------------------------------------------
#define SYSTICK_CSR     (0)  // SYST_CSR RW  -[a]    SysTick Control and Status Register
  #define SYSTICK_CSR_COUNTFLAG       (1 << 16)
  #define SYSTICK_CSR_CLKSOURCE_REF   0x0
  #define SYSTICK_CSR_CLKSOURCE_CPU   0x4
  #define SYSTICK_CSR_TICKINT_EN      0x2
  #define SYSTICK_CSR_ENABLE          0x1
#define SYSTICK_RVR     (4)  // SYST_RVR RW  Unknown SysTick Reload Value Register
#define SYSTICK_CVR     (8)  // SYST_CVR RW  Unknown SysTick Current Value Register
#define SYSTICK_CALIB   (12) // SYST_CALIB   RO  - [a] SysTick Calibration Value Register

//-----------------------------------------------------------------
// Systick: Timer device
//-----------------------------------------------------------------
class Systick: public Device
{
public:
    Systick(uint32_t base_addr, int irq_num)
    {
        m_base_addr  = base_addr;
        m_irq_number = irq_num;

        reset();
    }
    
    void reset(void)
    {
        m_irq         = false;
        m_reg_csr     = 0;
        m_reg_reload  = 0;
        m_reg_current = 0;
    }

    void write_reg(uint32_t address, uint32_t data)
    {
        switch (address)
        {
            case SYSTICK_CSR:
                m_reg_csr = data;                
            break;
            case SYSTICK_RVR:
                m_reg_reload = data;
            break;
            case SYSTICK_CVR:
                m_reg_current = data;
            break;
            default:
                fprintf(stderr, "Systick: Bad write @ %08x\n", address);
                exit (-1);
            break;
        }
    }
    uint32_t read_reg(uint32_t address)
    {
        uint32_t data = 0;

        switch (address)
        {
            case SYSTICK_CSR:
                data = m_reg_csr;

                // Clear overflow flag
                m_reg_csr &= ~SYSTICK_CSR_COUNTFLAG;
            break;
            case SYSTICK_RVR:
                data = m_reg_reload;
            break;
            case SYSTICK_CVR:
                data = m_reg_current;
            break;
            case SYSTICK_CALIB:
                data = 0;
            break;
            default:
                fprintf(stderr, "Systick: Bad read @ %08x\n", address);
                exit (-1);
            break;
        }
        return data;
    }

    int clock(void)
    {
        // Systick
        if (m_reg_csr & SYSTICK_CSR_ENABLE)
        {
            if (m_reg_current == 0)
            {
                m_reg_current = m_reg_reload;
                m_reg_csr    |= SYSTICK_CSR_COUNTFLAG;

                if (m_reg_csr & SYSTICK_CSR_TICKINT_EN)
                    m_irq = true;
            }
            else
                m_reg_current -= 1;
        }

        bool irq = m_irq;
        m_irq = false;
        return irq ? m_irq_number : -1;
    }

private:
    uint32_t m_base_addr;
    int      m_irq_number;
    bool     m_irq;
    uint32_t m_reg_csr;
    uint32_t m_reg_reload;
    uint32_t m_reg_current;
};

#endif
