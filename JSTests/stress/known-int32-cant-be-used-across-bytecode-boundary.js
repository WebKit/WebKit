//@ runDefault("--useConcurrentJIT=0")

function foo() {
    var x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13;
    var copy = [];
    var value = this[1];

    for (var p in this)
        copy[copy.length] = value;

    for (var i = 0; i < 1000; i++) {
        for (var j = 0; j < 1; j++) {
        }
        Math.min(0 ** []);
    }
};

noInline(foo);

let array0 = new Array(3).fill(1);
delete array0[0];

([])[1000] = 0xFFFFF;

for (var i = 0; i < 100; i++)
    foo.call(array0);
