//@ crashOK!
//@ skip if $hostOS == "windows"

$vm.crash();

throw new Error();
