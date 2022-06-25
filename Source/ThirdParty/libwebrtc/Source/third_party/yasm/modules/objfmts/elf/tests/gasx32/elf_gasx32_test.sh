#! /bin/sh
${srcdir}/out_test.sh elf_gasx32_test modules/objfmts/elf/tests/gasx32 "GAS elf-x32 objfmt" "-f elfx32 -p gas" ".o"
exit $?
