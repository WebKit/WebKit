//@ runDefault("--useAccessInlining=0")

function bar(ranges) {
    for (const [z] of ranges) {
        let ys = [];
        for (y = 0; y <= testLoopCount; y++) {
            ys[y] = false;
        }
    }
}

function foo() {
    let iterator = [][Symbol.iterator]();
    iterator.x = 1;
}

bar([ [], [], [], [], [], [], [], [], [], [], [] ]);
foo();
bar([ [], [] ]);
