//@ skip if ["arm", "mips"].include?($architecture)
//@ crashOK!

$vm.crash();

throw new Error();
