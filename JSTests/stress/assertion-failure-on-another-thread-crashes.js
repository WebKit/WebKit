//@ mustCrash!
//@ skip if $hostOS == "windows"
//@ runDefault

// The failing thread crashes after the script ends, so jsc must wait for it rather than exit normally.
$vm.failAssertionOnAnotherThread();
