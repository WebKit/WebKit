function assert(b) {
    if (!b)
        throw new Error("Bad")
}
noInline(assert);

function shallowEq(a, b) {
    assert(a.length === b.length);
    for (let i = 0; i < a.length; i++)
        assert(a[i] === b[i]);
}
noInline(shallowEq);

let tests = [
    [[1,2,3,4,5], [1,2,3,4,5], 0, 5],
    [[1,2,3,4,5], [1,2,3,4,5], 0],
    [[1,2,3,4,5], [4], -2, -1],
    [[1,2,3,4,5], [5], -1],
    [[1,2,3,4,5], [5], -1, 5],
    [[1,2,3,4,5], [], -10, -20],
    [[1,2,3,4,5], [], -20, -10],
    [[1,2,3,4,5], [], 6, 4],
    [[1,2,3,4,5], [], 3, 2],
    [[1,2,3,4,5], [4,5], 3, 10],
    [[1,2,3,4,5], [3,4,5], 2, 10],
    [[1,2,3,4,5], [1,2,3,4,5], -10, 10],
    [[1,2,3,4,5], [1,2,3,4,5], -5, 10],
    [[1,2,3,4,5], [2,3,4,5], -4, 10],
];

function runTest1(a, b) {
    return a.slice(b);
}
noInline(runTest1);

function runTest2(a, b, c) {
    return a.slice(b, c);
}
noInline(runTest2);

for (let i = 0; i < 10000; i++) {
    for (let [input, output, ...args] of tests) {
        assert(args.length === 1 || args.length === 2);
        if (args.length === 1)
            shallowEq(runTest1(input, args[0]), output);
        else
            shallowEq(runTest2(input, args[0], args[1]), output);
    }
}
