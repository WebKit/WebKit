function test(array) {
    var [a, b, c] = array;
    return a + b + c;
}
noInline(test);

var array = new Float32Array([1, 2, 3, 4]);
for (var i = 0; i < 2e6; ++i) {
    if (test(array) !== 6)
        throw new Error("bad result");
}
