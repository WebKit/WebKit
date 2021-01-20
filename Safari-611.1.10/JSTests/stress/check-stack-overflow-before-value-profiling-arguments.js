//@ skip if ["arm", "mips"].include?($architecture)
//@ runDefault("--useConcurrentJIT=0", "--thresholdForJITAfterWarmUp=10", "--slowPathAllocsBetweenGCs=10", "--useConcurrentGC=0")

function fullGC() {
    for (var i = 0; i < 10; i++) {
        new Float64Array(0x1000000);
    }
}

function outer() {
    function f() {
        try {
            const r = f();
        } catch(e) {
            const o = Object();
            function inner(a1, a2, a3) {
                try {
                    const r1 = new Uint32Array();
                    const r2 = r1.values();
                } catch(e2) {
                }
            }
            const result = inner();
        }
    }

    f();

    function edenGC() {
        for (let i = 0; i < 100; i++) {
            const floatArray = new Float64Array(0x10000);
        }
    }
    edenGC();
}

for (let i = 0; i < 100; i++) {
    const result = outer();
}

fullGC();

