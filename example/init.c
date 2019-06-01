#include "serial.h"

extern unsigned char _etext;
extern unsigned char _data;
extern unsigned char _edata;
extern unsigned char _bss;
extern unsigned char _ebss;

// Prototype the required startup functions
extern int main(void);

static inline void _exit(int exit_code)
{
    if (exit_code == 0)
        asm volatile("bkpt 0");
    else
        asm volatile("bkpt 1");
}

// The entry point of the application, prepare segments,
// initialize the cpu and execute main()
void boot_entry(void)
{
  register unsigned char *src, *dst;

#ifdef ARM_RELOCATE_DATA
  // Get physical data address and copy it to sram
  src = &_etext;
  dst = &_data;
  while(dst < &_edata)
    *dst++ = *src++;
#endif

  // Clear the bss segment
  dst = &_bss;
  while(dst < &_ebss)
    *dst++ = 0;

  // Setup serial port
  serial_init();

  // Execute the code at the program entry point
  int res = main();
  _exit(res);
}
