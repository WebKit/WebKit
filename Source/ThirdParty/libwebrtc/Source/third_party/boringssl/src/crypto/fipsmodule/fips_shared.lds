SECTIONS
{
  .text : {
    PROVIDE_HIDDEN(BORINGSSL_bcm_text_start = .);
    *(.text)
    *(.text.unlikely.*)
    /* C++ `inline` symbols are normally placed in their own sections to be
     * deduplicated. Undo this. */
    *(.text.*)
    PROVIDE_HIDDEN(BORINGSSL_bcm_text_end = .);
  }
  .rodata : {
    PROVIDE_HIDDEN(BORINGSSL_bcm_rodata_start = .);
    *(.rodata)
    *(.rodata.*)
    PROVIDE_HIDDEN(BORINGSSL_bcm_rodata_end = .);
  }

  /* When multiple sections are merged, their corresponding relocation sections
   * need to be merged as well. Older architectures (e.g. 32-bit Arm) tend to
   * use .rel and newer ones (e.g. 64-bit Arm) tend to use .rela. */
  .rel.text : {
    *(.rel.text)
    *(.rel.text.unlikely.*)
    *(.rel.text.*)
  }
  .rela.text : {
    *(.rela.text)
    *(.rela.text.unlikely.*)
    *(.rela.text.*)
  }
  .rel.rodata : {
    *(.rel.rodata)
    *(.rel.rodata.*)
  }
  .rela.rodata : {
    *(.rela.rodata)
    *(.rela.rodata.*)
  }

  /DISCARD/ : {
    /* These sections shouldn't exist. In order to catch any slip-ups, direct
     * the linker to discard them. */
    *(.rel.dyn)
    *(.rela.dyn)
    *(.data)
    *(.rel.ro)
    *(.rela.ro)
    *(*.data.*)
  }
}
