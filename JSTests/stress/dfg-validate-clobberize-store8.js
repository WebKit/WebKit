//@ skip if not $isWasmPlatform
//@ runDefault("--thresholdForOptimizeAfterWarmUp=100", "--thresholdForOptimizeAfterLongWarmUp=100", "--thresholdForFTLOptimizeAfterWarmUp=1000", "--useConcurrentJIT=0", "--validateDFGClobberize=1")

const v1 = WebAssembly.Memory;
const o4 = {
    "initial": 1,
    "maximum": 1,
};
const v5 = new v1(o4);
const v6 = WebAssembly.Instance;
const v7 = WebAssembly.Module;
const v122 = new Uint8Array([0,97,115,109,1,0,0,0,1,7,1,96,3,127,127,127,0,2,12,1,2,106,115,3,109,101,109,2,1,1,1,3,2,1,0,6,1,0,7,13,1,9,100,111,95,109,101,109,99,112,121,0,0,10,57,1,55,1,1,127,65,0,33,3,3,64,2,64,32,2,32,3,70,13,0,32,1,65,4,108,32,3,65,4,108,106,32,0,32,3,65,4,108,106,40,0,0,54,0,0,32,3,65,1,106,33,3,12,1,11,11,11]);
const v123 = new v7(v122);
const o124 = {
    "mem": v5,
};
const o125 = {
    "js": o124,
};
const v126 = new v6(v123, o125);
for (let i128 = 0; i128 < 10000; i128++) {
    v126.exports.do_memcpy();
}
gc();
