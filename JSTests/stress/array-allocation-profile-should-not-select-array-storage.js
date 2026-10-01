//@ $skipModes << :lockdown

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function createWithPush() {
    let array = [];
    for (let i = 0; i < 8; ++i)
        array.push({ i });
    return array;
}

function createWithLiteral() {
    return [{}, {}, {}];
}

function createWithNewArray() {
    let array = new Array(8);
    for (let i = 0; i < 8; ++i)
        array[i] = { i };
    return array;
}

let source = createWithPush();

function createWithMap() {
    return source.map((value) => value);
}

function createWithFilter() {
    return source.filter(() => true);
}

function createInt32WithPush() {
    let array = [];
    for (let i = 0; i < 8; ++i)
        array.push(i);
    return array;
}

function createDoubleWithPush() {
    let array = [];
    for (let i = 0; i < 8; ++i)
        array.push(i + 0.5);
    return array;
}

function createInt32WithNewArray() {
    let array = new Array(8);
    for (let i = 0; i < 8; ++i)
        array[i] = i;
    return array;
}

function createInt32WithConstantLiteral() {
    return [1, 2, 3];
}

function createEmpty() {
    return [];
}

function test(create, expected) {
    for (let i = 0; i < testLoopCount; ++i) {
        let array = create();
        if (!(i % 100))
            Object.freeze(array);
    }
    fullGC();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe($vm.indexingMode(create()), expected);
}

test(createWithPush, "ArrayWithContiguous");
test(createWithLiteral, "ArrayWithContiguous");
test(createWithNewArray, "ArrayWithContiguous");
test(createWithMap, "ArrayWithContiguous");
test(createWithFilter, "ArrayWithContiguous");
test(createInt32WithPush, "ArrayWithInt32");
test(createDoubleWithPush, "ArrayWithDouble");
test(createInt32WithNewArray, "ArrayWithInt32");
test(createInt32WithConstantLiteral, "CopyOnWriteArrayWithInt32");
test(createEmpty, "ArrayWithUndecided");
