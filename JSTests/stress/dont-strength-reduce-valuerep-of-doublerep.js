let a2 = [];
let thingy = {length: 2**55, __proto__: []};
let func = (x) => x;

noInline(Array.prototype.map);

// This test should not crash.
for (let i = 0; i < 100000; ++i) {
    try {
        if (i > 0 && (i % 1000) === 0)
            thingy.map(func)
        a2.map(func);
    } catch(e) {
    }
}
