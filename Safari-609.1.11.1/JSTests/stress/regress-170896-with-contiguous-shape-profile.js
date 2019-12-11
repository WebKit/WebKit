function test() {
    let a = [,,,,,,,,,];
    return a.concat();
}
noInline(test);

test()[0] = {}; // Set the ArrayAllocationProfile to ContiguousShape.

for (let i = 0; i < 20000; ++i) {
    var result = test();
    if (result[0])
        throw result.toString();
}
