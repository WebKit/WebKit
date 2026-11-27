function getIterator(object) {
    return object[Symbol.iterator];
}
noInline(getIterator);

function getCustom(object) {
    return object[Symbol.asyncIterator];
}
noInline(getCustom);

function* generator() { }
let objects = [
    [1],
    new Set([1]),
    new Map([[1, 2]]),
    new Uint8Array(2),
    (function () { return arguments; })(1),
    { [Symbol.iterator]: generator },
    Object.create({ [Symbol.iterator]: generator }),
    { get [Symbol.iterator]() { return generator; } },
];
let expected = objects.map((object) => object[Symbol.iterator]);
let proxy = new Proxy({ }, { get(target, key) { return key === Symbol.iterator ? 42 : undefined; } });

let prototype = { [Symbol.asyncIterator]: 1 };
let derived = [];
for (let i = 0; i < 12; ++i) {
    let object = Object.create(prototype);
    object["property" + i] = i;
    derived.push(object);
}
let own = { [Symbol.asyncIterator]: 2 };

for (let i = 0; i < testLoopCount; ++i) {
    let index = i % objects.length;
    if (getIterator(objects[index]) !== expected[index])
        throw new Error("getIterator(objects[" + index + "]) is wrong");
    if (!(i % 16) && getIterator(proxy) !== 42)
        throw new Error("getIterator(proxy) is wrong");
    if (getCustom(derived[i % derived.length]) !== 1)
        throw new Error("getCustom(derived) is wrong before the prototype changes");
    if (getCustom(own) !== 2)
        throw new Error("getCustom(own) is wrong");
}

prototype[Symbol.asyncIterator] = 3;
Object.defineProperty(derived[0], Symbol.asyncIterator, { get() { return 4; } });
for (let i = 0; i < testLoopCount; ++i) {
    let index = i % derived.length;
    if (getCustom(derived[index]) !== (index ? 3 : 4))
        throw new Error("getCustom(derived[" + index + "]) did not see the new property");
}
