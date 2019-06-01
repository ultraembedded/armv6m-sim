#include "exception.h"

// Declare a weak alias macro as described in the GCC manual[1][2]
#define WEAK_ALIAS(f) __attribute__ ((weak, alias (#f)));
#define SECTION(s) __attribute__ ((section(s)))

/******************************************************************************
 * Forward undefined IRQ handlers to an infinite loop function. The Handlers
 * are weakly aliased which means that (re)definitions will overide these.
 *****************************************************************************/
void irq_undefined() 			{ while(1); }

void fault_handler (void) __attribute__ ((weak));
void svcall_handler (void) __attribute__ ((weak));
void pendsv_handler (void) __attribute__ ((weak));
void systick_handler (void) __attribute__ ((weak));

void fault_handler (void)   { while (1); }
void svcall_handler (void)  { while (1); }
void pendsv_handler (void)  { while (1); }
void systick_handler (void) { while (1); }

/******************************************************************************
 * Forward undefined IRQ handlers to an infinite loop function. The Handlers
 * are weakly aliased which means that (re)definitions will overide these.
 *****************************************************************************/

// Prototype the entry values, which are handled by the linker script
extern void* stack_entry;
extern void boot_entry(void);

// Defined irq vectors using simple c code following the description in a white 
// paper from ARM[3] and code example from Simonsson Fun Technologies[4].
// These vectors are placed at the memory location defined in the linker script
void *vectors[] SECTION(".irq_vectors") =
{
  // Stack and program reset entry point
  &stack_entry,          // The initial stack pointer
  boot_entry,            // The reset handler

  // Various fault handlers
  irq_undefined,         // The NMI handler
  fault_handler,         // The hard fault handler
  fault_handler,         // MemManage_Handler
  fault_handler,         // BusFault_Handler
  fault_handler,         // UsageFault_Handler
  0,                     // Reserved
  0,                     // Reserved
  0,                     // Reserved
  0,                     // Reserved
  svcall_handler,         // SVCall handler
  irq_undefined,         // DebugMon_Handler
  0,                     // Reserved
  pendsv_handler,         // The PendSV handler
  systick_handler,         // The SysTick handler
  
  // Wakeup I/O pins handlers
  irq_undefined,     // PIO0_0  Wakeup
  irq_undefined,     // PIO0_1  Wakeup
  irq_undefined,     // PIO0_2  Wakeup
  irq_undefined,     // PIO0_3  Wakeup
  irq_undefined,     // PIO0_4  Wakeup
  irq_undefined,     // PIO0_5  Wakeup
  irq_undefined,     // PIO0_6  Wakeup
  irq_undefined,     // PIO0_7  Wakeup
  irq_undefined,     // PIO0_8  Wakeup
  irq_undefined,     // PIO0_9  Wakeup
  irq_undefined,     // PIO0_10 Wakeup
  irq_undefined,     // PIO0_11 Wakeup
  irq_undefined,     // PIO1_0  Wakeup
  
  // Specific peripheral irq handlers
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
  irq_undefined,
};
