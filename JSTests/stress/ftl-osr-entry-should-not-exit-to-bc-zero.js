//@ runDefault("--verboseOSRExitFuzz=0", "--useOSRExitFuzz=1", "--fireOSRExitFuzzAtOrAfter=10", "--jitPolicyScale=0", "--useConcurrentJIT=0")

let a = [0, 0, 0];
a.sort();
a.sort();
