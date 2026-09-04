//@ mustCrashWith!(:trap, "AAAAAHHHH")
//@ runFTLNoCJIT

$vm.crash("AAAAAHHHH");
