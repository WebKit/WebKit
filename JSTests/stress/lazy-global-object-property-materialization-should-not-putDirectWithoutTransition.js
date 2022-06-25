//@ runDefault("--useConcurrentJIT=0")

const x = {};
for (let i = 0; i < 100; i++) {
    const o = createGlobalObject();
    x.__proto__ = o;
    for (let j = 0; j < 10000; j++) {
        o.y = 0;
    }
    with (x) {
        new Uint8Array();
    }
}
