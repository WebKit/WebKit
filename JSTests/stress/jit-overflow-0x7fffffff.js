testLoopCount = 100;

function test(a) {
    return a + 0x7fffffff + 1.1 & 0x7fffffff | a;
}

noInline(test);

// Force JIT compilation
for (let i = 0; i < 100000; i++) {
    test(3);
}

for (var i = 0; i < testLoopCount; ++i) {
    var result = test(1);
    if (result != 1)
        throw new Error("bad result: " + result);
}