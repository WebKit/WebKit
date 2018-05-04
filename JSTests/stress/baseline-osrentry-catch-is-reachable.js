// Regression test for bug 185281. This should terminate without throwing.

// These values are added to increase bytecode count.
let foo = {};
foo.x = null;
foo.y = null;
let z = null;
let z2 = {};

for (var i = 0; i <= 10; i++) {
    for (var j = 0; j <= 100; j++) {
        try {
            xxx;
            for (;;) {}
        } catch (e) {}
    }
}
