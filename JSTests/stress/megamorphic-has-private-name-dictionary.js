function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + String(actual) + ", expected: " + String(expected));
}

class Identity {
    constructor(object) {
        return object;
    }
}

class Stamper extends Identity {
    #field = 42;

    static has(object) {
        return #field in object;
    }
}
noInline(Stamper.has);

let objects = [];
for (let i = 0; i < 64; ++i) {
    let object = { ["property" + i]: i };
    if (i & 1)
        new Stamper(object);
    objects.push(object);
}
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Stamper.has(objects[i & 63]), !!(i & 1));

// The structure of a cacheable dictionary does not change when the private name is added.
for (let i = 0; i < 32; ++i)
    $vm.toCacheableDictionary(objects[i]);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Stamper.has(objects[i & 63]), !!(i & 1));
for (let i = 0; i < 32; i += 2)
    new Stamper(objects[i]);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Stamper.has(objects[i & 63]), (i & 63) < 32 || !!(i & 1));

// Flattening a dictionary keeps its structure, which is no longer a dictionary afterwards.
for (let i = 32; i < 48; ++i) {
    $vm.toCacheableDictionary(objects[i]);
    $vm.flattenDictionaryObject(objects[i]);
}
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Stamper.has(objects[i & 63]), (i & 63) < 32 || !!(i & 1));
for (let i = 32; i < 48; i += 2)
    new Stamper(objects[i]);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Stamper.has(objects[i & 63]), (i & 63) < 48 || !!(i & 1));

$vm.toUncacheableDictionary(objects[62]);
$vm.toUncacheableDictionary(objects[63]);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(Stamper.has(objects[i & 63]), (i & 63) < 48 || !!(i & 1));
new Stamper(objects[62]);
shouldBe(Stamper.has(objects[62]), true);
shouldBe(Stamper.has(objects[63]), true);
