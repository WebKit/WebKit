function test() {
    let a = [,,,,,,,,,];
    return a.concat();
}
noInline(test);

test()[0] = 1.2345; // Set the ArrayAllocationProfile to DoubleShape.

for (let i = 0; i < 20000; ++i) {
    var result = test();
    if (result[0])
        throw result.toString();
}
