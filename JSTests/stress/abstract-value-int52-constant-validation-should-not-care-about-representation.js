//@ runDefault("--validateAbstractInterpreterState=1", "--validateAbstractInterpreterStateProbability=1.0", "--useConcurrentJIT=0")

let ab = new ArrayBuffer(4);
let dv = new DataView(ab);
for (let i = 0; i < 2000; i++) {
    dv.setUint32(0, 0);
    for (let j = 0; j < 1000; ++j) {
    }
}
