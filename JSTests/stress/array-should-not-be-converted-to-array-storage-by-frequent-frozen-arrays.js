//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function create(length) {
    let array = new Array(length);
    for (let i = 0; i < length; ++i)
        array[i] = { i };
    return array;
}
noInline(create);

function createFrozen() {
    return Object.freeze([{ i: 0 }, { i: 1 }, { i: 2 }]);
}
noInline(createFrozen);

function isObject(value) {
    return typeof value === "object";
}

function every(array) {
    return array.every(isObject);
}

function some(array) {
    return array.some((value) => value.i === -1);
}

function map(array) {
    return array.map(isObject).length;
}

function filter(array) {
    return array.filter(isObject).length;
}

function forEach(array) {
    let count = 0;
    array.forEach(() => { ++count; });
    return count;
}

function at(array) {
    return array.at(0).i;
}

function indexed(array) {
    let sum = 0;
    for (let i = 0; i < array.length; ++i)
        sum += array[i].i;
    return sum;
}

function forOf(array) {
    let sum = 0;
    for (let value of array)
        sum += value.i;
    return sum;
}

function destructure(array) {
    let [first, second] = array;
    return first.i + second.i;
}

function store(array) {
    for (let i = 0; i < array.length; ++i)
        array[i] = array[0];
}

function push(array) {
    try {
        array.push(array[0]);
    } catch { }
}

function test(func) {
    for (let i = 0; i < testLoopCount; ++i) {
        let array = create(8);
        func(array);
        shouldBe($vm.indexingMode(array), "ArrayWithContiguous");
        if (i % 2)
            func(createFrozen());
    }
}

for (let func of [every, some, map, filter, forEach, at, indexed, forOf, destructure, store, push])
    test(func);
