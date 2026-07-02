libyasm_a_SOURCES += modules/preprocs/gas/gas-preproc.c
libyasm_a_SOURCES += modules/preprocs/gas/gas-eval.h
libyasm_a_SOURCES += modules/preprocs/gas/gas-eval.c

YASM_MODULES += preproc_gas

EXTRA_DIST += modules/preprocs/gas/tests/Makefile.inc

include modules/preprocs/gas/tests/Makefile.inc
