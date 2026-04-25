//@ runDefault("--thresholdForOptimizeAfterWarmUp=0", "--thresholdForOptimizeSoon=0", "--thresholdForFTLOptimizeAfterWarmUp=0")
function f3(a4) {
    const o7 = {
        ["forEach"]: "pCGSxWy10A",
        set e(a6) {
        },
    };
    return a4;
}
f3("forEach");
f3("pCGSxWy10A");
f3("function");
const v12 = new Int8Array();
const v14 = new Uint8ClampedArray(v12);
for (const v15 in "pCGSxWy10A") {
    for (let v16 = 0; v16 < 100; v16++) {
        for (let v18 = 0; v18 < 10; v18++) {
            try {
                (2147483649).toString(v16);
            } catch(e20) {
            }
        }
    }
}
f3(v12);
gc();