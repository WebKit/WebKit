//@ skip if $memoryLimited
//@ runDefault("--validateAbstractInterpreterState=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")

const s = 'A'.repeat(2**25 * 5);
const o = {};
for (let i = 0; i < 1; i++) {
    o[s];
    for (let j = 0; j < 10; j++) {
    }
}
