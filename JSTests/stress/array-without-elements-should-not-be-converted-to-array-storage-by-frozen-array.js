//@ skip if not $jitTests
//@ runDefault("--validateOptions=true", "--useConcurrentJIT=0")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldNotBeCompiledRepeatedly(func) {
    if (numberOfDFGCompiles(func) > 3)
        throw new Error(`${func.name} was compiled ${numberOfDFGCompiles(func)} times`);
}

let frozen = Object.freeze([]);

function first(array) {
    return array[0];
}
noInline(first);

function indexed(array) {
    let count = 0;
    for (let i = 0; i < array.length; ++i) {
        if (array[i] === undefined)
            ++count;
    }
    return count;
}
noInline(indexed);

function forOf(array) {
    let count = 0;
    for (let value of array)
        ++count;
    return count;
}
noInline(forOf);

function destructure(array) {
    let [first, second] = array;
    return first;
}
noInline(destructure);

function store(array) {
    array[0] = 42;
}
noInline(store);

function push(array) {
    try {
        array.push(42);
    } catch { }
}
noInline(push);

function test(func, create, expected) {
    for (let i = 0; i < testLoopCount; ++i) {
        let array = create();
        func(array);
        shouldBe($vm.indexingMode(array), expected);
        if (!(i % 100))
            func(frozen);
    }
    shouldNotBeCompiledRepeatedly(func);
}

test(first, () => [], "ArrayWithUndecided");
test(indexed, () => new Array(4), "ArrayWithUndecided");
test(forOf, () => new Array(4), "ArrayWithUndecided");
test(destructure, () => new Array(4), "ArrayWithUndecided");
test(store, () => [], "ArrayWithInt32");
test(push, () => [], "ArrayWithInt32");
