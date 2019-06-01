MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00010000
  SRAM(rwx): ORIGIN = 0x20000000, LENGTH = 8M
}

sram_top = ORIGIN(SRAM) + LENGTH(SRAM);
ENTRY(boot_entry)

SECTIONS
{
  .text :
  {
    KEEP(*(.irq_vectors))
    *(.text*)
    *(.rodata*)
  } > FLASH

  /*
   * More information about Special Section Indexes is available in the
   * free "ELF for the ARM Architecture" document from ARM Limited
   * http://simplemachines.it/doc/aaelf.pdf
   *
   */
  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > SRAM
  __exidx_start = .;
  .ARM.exidx : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > SRAM
  __exidx_end = .;

  _etext = .;
    
  .data :
  {
    _data = .;
    *(vtable)
    *(.data*)
    _edata = .;
  } > SRAM AT> FLASH

  /* zero initialized data */
  .bss :
  {
    _bss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > SRAM
  
  end = .;

  /* For GDB compatibility we decrease the top with 16 bytes */
  stack_entry = sram_top - 16;
}
