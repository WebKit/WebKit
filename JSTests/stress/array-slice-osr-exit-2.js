function assert(b) {
    if (!b)
        throw new Error("Bad")
}
noInline(assert);

class Foo extends Array {
    constructor(...args) {
        super(...args);
    }
};
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
    let result = a.slice(b);
    assert(a instanceof Foo === result instanceof Foo);
    return result;
}
noInline(runTest1);

function runTest2(a, b, c) {
    let result = a.slice(b, c);
    assert(a instanceof Foo === result instanceof Foo);
    return result;
}
noInline(runTest2);

function addRandomProperties(input) {
    for (let i = 0; i < 4; i++) {
        input["prop" + i + ((Math.random() * 100000) | 0)] = i;
    }
}
noInline(addRandomProperties);

function runTests() {
    for (let i = 0; i < 10000; i++) {
        for (let [input, output, ...args] of tests) {
            addRandomProperties(input);
            assert(args.length === 1 || args.length === 2);
            if (args.length === 1)
                shallowEq(runTest1(input, args[0]), output);
            else
                shallowEq(runTest2(input, args[0], args[1]), output);
        }
    }
}

runTests();

tests.push([new Foo(1,2,3,4,5), [1,2,3,4,5], -10, 10]);
tests.push([new Foo(1,2,3,4,5), [1,2,3,4,5], -5, 10]);
tests.push([new Foo(1,2,3,4,5), [2,3,4,5], -4, 10]);
tests.push([new Foo(1,2,3,4,5), [2,3,4,5], -4]);
runTests();
