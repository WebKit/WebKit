//@ skip
//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0")
const a = [];
a.length = 65532;
Object.constructor(...a)();
