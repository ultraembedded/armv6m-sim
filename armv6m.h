#ifndef __ARMV6M_H__
#define __ARMV6M_H__

#include <stdint.h>
#include <vector>
#include "armv6m_opcodes.h"
#include "memory.h"
#include "systick.h"
#include "sysuart.h"

//-------------------------------------------------------------------
// Defines:
//-------------------------------------------------------------------
#define REG_SP      13
#define REG_LR      14
#define REG_PC      15
#define REGISTERS   16

#define APSR_N_SHIFT        (31)
#define APSR_Z_SHIFT        (30)
#define APSR_C_SHIFT        (29)
#define APSR_V_SHIFT        (28)

#define APSR_N          (1<<APSR_N_SHIFT)
#define APSR_Z          (1<<APSR_Z_SHIFT)
#define APSR_C          (1<<APSR_C_SHIFT)
#define APSR_V          (1<<APSR_V_SHIFT)

#define ALL_FLAGS   (APSR_N | APSR_Z | APSR_C | APSR_V)
#define FLAGS_NZC   (APSR_N | APSR_Z | APSR_C)

#define PRIMASK_PM          (1 << 0)

#define CONTROL_NPRIV       (1 << 0)
#define CONTROL_SPSEL       (1 << 1)
#define CONTROL_MASK        (CONTROL_NPRIV | CONTROL_SPSEL)

typedef enum { MODE_THREAD = 0, MODE_HANDLER } tMode;

#define EXC_RETURN          0xFFFFFFE0

//--------------------------------------------------------------------
// Defines:
//--------------------------------------------------------------------
#define LOG_FETCH       (1 << 0)
#define LOG_REGISTERS   (1 << 1)
#define LOG_PUSHPOP     (1 << 2)
#define LOG_MEM         (1 << 3)
#define LOG_INST        (1 << 4)
#define LOG_FLAGS       (1 << 5)

#define MAX_MEM_REGIONS     16

typedef void (*FP_SIM_STEP)(void *p);

//--------------------------------------------------------------------
// Armv6m: Simple ARM v6m model
//--------------------------------------------------------------------
class Armv6m
{
public:
                        Armv6m(uint32_t baseAddr = 0, uint32_t len = 0);
    virtual             ~Armv6m();

    bool                create_memory(uint32_t addr, uint32_t size, uint8_t *mem = NULL);
    bool                attach_memory(Memory *memory, uint32_t baseAddr, uint32_t len);

    bool                valid_addr(uint32_t address);
    void                write(uint32_t address, uint8_t data);
    void                write32(uint32_t address, uint32_t data);
    void                write_mem(uint32_t address, uint32_t data, int width);
    uint8_t             read(uint32_t address);
    uint32_t            read32(uint32_t address);
    uint32_t            read_mem(uint32_t address, int width);

    void                reset(uint32_t start_addr);
    uint32_t            get_opcode(uint32_t pc);
    void                step(void);

    void                set_step_callback(FP_SIM_STEP cb, void *arg) { m_step_cb = cb; m_step_cb_arg = arg; }

    void                set_interrupt(int irq);

    bool                get_fault(void)      { return m_fault; }
    bool                get_stopped(void)    { return false; }
    bool                get_reg_valid(int r) { return true; }
    uint32_t            get_register(int r);

    uint32_t            get_pc(void)      { return m_regfile[REG_PC]; }
    uint32_t            get_opcode(void)  { return get_opcode(get_pc()); }
    int                 get_num_reg(void) { return 16; }

    void                set_register(int r, uint32_t val);
    void                set_pc(uint32_t val);
    void                set_flag(uint32_t flag, bool val);
    bool                get_flag(uint32_t flag);

    // Breakpoints
    bool                get_break(void);
    bool                set_breakpoint(uint32_t pc);
    bool                clr_breakpoint(uint32_t pc);
    bool                check_breakpoint(uint32_t pc);

    void                enable_trace(uint32_t mask)                 { m_trace = mask; }

    bool                error(bool terminal, const char *fmt, ...);

protected:
    uint16_t            armv6m_read_inst(uint32_t addr);
    void                armv6m_update_sp(uint32_t sp);
    void                armv6m_update_n_z_flags(uint32_t rd);
    uint32_t            armv6m_add_with_carry(uint32_t rn, uint32_t rm, uint32_t carry_in, uint32_t mask);
    uint32_t            armv6m_shift_left(uint32_t val, uint32_t shift, uint32_t mask);
    uint32_t            armv6m_shift_right(uint32_t val, uint32_t shift, uint32_t mask);
    uint32_t            armv6m_arith_shift_right(uint32_t val, uint32_t shift, uint32_t mask);
    uint32_t            armv6m_rotate_right(uint32_t val, uint32_t shift, uint32_t mask);
    uint32_t            armv6m_sign_extend(uint32_t val, int offset);
    void                armv6m_dump_inst(uint16_t inst);
    uint32_t            armv6m_exception(uint32_t pc, uint32_t exception);
    void                armv6m_exc_return(uint32_t pc);

public:
    int                 armv6m_decode(uint16_t inst);
    void                armv6m_execute(uint16_t inst, uint16_t inst2);

protected:  

    // Registers
    uint32_t            m_regfile[REGISTERS];
    uint32_t            m_psp; // Process stack pointer
    uint32_t            m_msp; // Main stack pointer
    uint32_t            m_apsr;
    uint32_t            m_ipsr;
    uint32_t            m_epsr;

    uint32_t            m_primask;
    uint32_t            m_control;
    tMode               m_current_mode;

    uint32_t            m_entry_point;

    // Decode
    int                 m_inst_group;
    uint32_t            m_rd;
    uint32_t            m_rt;
    uint32_t            m_rm;
    uint32_t            m_rn;
    uint32_t            m_imm;
    uint32_t            m_cond;
    uint32_t            m_reglist;

private:


    // Memory
    Memory             *m_mem[MAX_MEM_REGIONS];
    uint32_t            m_mem_base[MAX_MEM_REGIONS];
    uint32_t            m_mem_size[MAX_MEM_REGIONS];
    int                 m_mem_regions;

    // Status
    bool                m_fault;
    bool                m_break;
    int                 m_trace;

    // Breakpoints
    bool                m_has_breakpoints;
    std::vector <uint32_t > m_breakpoints;

    // Systick
    Systick            *m_systick;

    // UART
    Sysuart            *m_uart;

    FP_SIM_STEP         m_step_cb;
    void               *m_step_cb_arg;
};

#endif
