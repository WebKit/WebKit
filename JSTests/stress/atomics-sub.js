//@ runDefault("--useConcurrentJIT=0", "--jitPolicyScale=0.01")
const arr = new Int8Array(16);
let prev = 0;
for (let i = 129; i > 0; i--) {
    const r = this.Atomics.sub(arr, 0, i);
    if (i < 129 && prev == r)
        throw Error("bad");
    prev = r;
}

