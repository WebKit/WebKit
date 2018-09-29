#! /bin/sh
${srcdir}/out_test.sh elf_x32_test modules/objfmts/elf/tests/x32 "elf-x32 objfmt" "-f elf -m x32" ".o"
exit $?
