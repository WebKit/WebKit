//@ runDefault("--useConcurrentJIT=0")
function trigger() {
    var v21 = "test";
    var v25 = (10)[11];
    var v27 = 4294967297 + v21;
    for (let v31 = 1; v31 < 1000; v31++) {
        ;
    }
    v33 = new Float32Array();
    v34 = v21[v27];
    v35 = v33[v27];
}

function main() {
    for (let i = 0; i < 10000; i++) {
        trigger();
    }
}

main();
