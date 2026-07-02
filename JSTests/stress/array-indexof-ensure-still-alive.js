//@ runDefault("--useFTLJIT=1", "--jitPolicyScale=0.1", "--useConcurrentJIT=0", "--useConcurrentGC=0", "--sweepSynchronously=1", "--collectContinuously=1")

function opt(s, needle) {
    return [s + "A", s + "B", s + "C", s + "D", s + "E", s + "F", s + "G", s + "H"].indexOf(needle);
}
noInline(opt);

let big = "Q".repeat(1024 * 1024);
let needleStr = "Z".repeat(big.length + 1);

for (let i = 0; i < 2000; i++)
    opt(big, (i & 1) ? needleStr : 1234);

for (let i = 0; i < 10000; i++)
    opt(big, needleStr);
