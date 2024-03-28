//@ crashOK!
//@ skip if $hostOS == "windows"
//@ runDefault

$vm.crash();

throw new Error();
