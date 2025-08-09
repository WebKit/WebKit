function test(a) {
    return a + 0x7fffffff + 1.1 & 0x7fffffff | a;
}

noInline(test);

for (var i = 0; i < testLoopCount; ++i) {
    var result = test(1);
    if (result != 1)
        throw new Error("bad result: " + result);
} 