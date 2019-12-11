//@ runDefault
// This test should not crash.

var objs;

for (let i = 0; i < 500; i += 100) {
    objs = [];
    gc();

    // Make "Retired" blocks.
    for (let j = 0; j < i; j++) {
        let o;
        switch (i % 6) {
        case 0: o = { };
        case 1: o = { a: i };
        case 2: o = { a: i, b: i};
        case 3: o = { a: i, b: i, c: i };
        case 4: o = { a: i, b: i, c: i, d: i };
        case 5: o = { a: i, b: i, c: i, d: i, e: i };
        }
        objs[j] = o;
    }
    edenGC();
}
