YASM_ADD_MODULE(objfmt_macho
    objfmts/macho/macho-objfmt.c
    )
list(APPEND YASM_MODULES objfmt_macho32)
list(APPEND YASM_MODULES objfmt_macho64)
