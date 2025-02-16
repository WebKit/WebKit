function test(v) {
    let sum = 0;
    for (var p in v)
        sum += v[p];
    return sum;
}

Number.prototype[2] = 4;
for (let i = 0; i < testLoopCount; ++i) {
    if (test(0) !== 4)
        throw new Error();
}

String.prototype[2] = 5;
for (let i = 0; i < testLoopCount; ++i) {
    var result = test("");
    if (result !== 5)
        throw new Error();

    result = test("a");
    if (result !== "0a5")
        throw new Error();
}
