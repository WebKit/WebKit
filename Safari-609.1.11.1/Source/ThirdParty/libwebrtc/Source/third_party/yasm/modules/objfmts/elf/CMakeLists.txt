YASM_ADD_MODULE(objfmt_elf
    objfmts/elf/elf.c
    objfmts/elf/elf-objfmt.c
    objfmts/elf/elf-x86-x86.c
    objfmts/elf/elf-x86-amd64.c
    objfmts/elf/elf-x86-x32.c
    )
list(APPEND YASM_MODULES objfmt_elf32)
list(APPEND YASM_MODULES objfmt_elf64)
list(APPEND YASM_MODULES objfmt_elfx32)
