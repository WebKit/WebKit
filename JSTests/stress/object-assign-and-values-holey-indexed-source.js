function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

Object.prototype[4] = "inherited";

function holey(make) {
    return { 3: make(3), 50: make(50), 100: make(100), name: make(0) };
}

function punched(make) {
    let object = {};
    for (let i = 0; i < 8; ++i)
        object[i] = make(i);
    delete object[0];
    delete object[5];
    delete object[7];
    return object;
}

function assign(target, source) {
    return Object.assign(target, source);
}
noInline(assign);

function values(object) {
    return Object.values(object);
}
noInline(values);

function check(source, keys) {
    let expectedKeys = keys.join();
    let expectedValues = keys.map((key) => source[key]);

    let result = assign({}, source);
    shouldBe(Object.keys(result).join(), expectedKeys);
    shouldBe(Object.hasOwn(result, 4), keys.includes(4));
    for (let key of keys)
        shouldBe(result[key], source[key]);

    result = assign({ 4: "own", 6: 0.5, 50: "old", 600: 1 }, source);
    shouldBe(result[4], keys.includes(4) ? source[4] : "own");
    shouldBe(result[6], keys.includes(6) ? source[6] : 0.5);
    shouldBe(result[600], 1);
    for (let key of keys)
        shouldBe(result[key], source[key]);

    shouldBe(assign(source, source), source);
    shouldBe(Object.keys(source).join(), expectedKeys);

    let array = values(source);
    shouldBe(array.length, keys.length);
    for (let i = 0; i < keys.length; ++i)
        shouldBe(array[i], expectedValues[i]);
}

let makers = [
    (i) => i + 1,
    (i) => i + 0.5,
    (i) => "value" + i,
    (i) => i === 3 ? undefined : i,
    (i) => i === 3 ? null : i + 0.5,
];

let cases = [];
for (let make of makers) {
    cases.push([holey(make), [3, 50, 100, "name"]]);
    cases.push([punched(make), [1, 2, 3, 4, 6]]);
}

for (let i = 0; i < testLoopCount; ++i) {
    let [source, keys] = cases[i % cases.length];
    check(source, keys);
}
