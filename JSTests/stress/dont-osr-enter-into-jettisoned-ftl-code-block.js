//@ runDefault("--useRandomizingFuzzerAgent=1", "--validateAbstractInterpreterState=1", "--jitPolicyScale=0", "--useConcurrentJIT=0", "--validateAbstractInterpreterStateProbability=1.0")

let x = [];
let k = 1;
z = 0;

for (var i = 0; i < 36; i++) {
    k = k * 2;
    x[k - 2] = k;
}

for (var j = 0; j === -1; j++) {
    z = z;
}
