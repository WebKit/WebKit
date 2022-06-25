function test() {
    let a = [,,,,,,,,,];
    return a.concat();
}
noInline(test);

test()[0] = 42; // Set the ArrayAllocationProfile to Int32Shape.

for (let i = 0; i < 20000; ++i) {
    var result = test();
    if (result[0])
        throw result.toString();
}
