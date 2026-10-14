function storesAroundExit(o, i) {
    o.a = 1;
    let r = i + 1;
    o.a = 2;
    return r;
}
noInline(storesAroundExit);

function elementStoresAroundExit(array, i) {
    array[0] = 1;
    let r = i + 1;
    array[0] = 2;
    return r;
}
noInline(elementStoresAroundExit);

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

let object = { a: 0 };
let array = [0, 0];
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(storesAroundExit(object, i), i + 1);
    shouldBe(elementStoresAroundExit(array, i), i + 1);
}

let seen;
shouldBe(storesAroundExit(object, { valueOf() { seen = object.a; return 41; } }), 42);
shouldBe(seen, 1);
shouldBe(object.a, 2);

seen = undefined;
shouldBe(elementStoresAroundExit(array, { valueOf() { seen = array[0]; return 41; } }), 42);
shouldBe(seen, 1);
shouldBe(array[0], 2);
