//@ $skipModes << :lockdown
// Lockdown has no inline caches, so nothing flattens a dictionary there.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + expected + " but got " + actual);
}

function isDictionary(object) {
    return describe(object).includes("Dictionary");
}

// Each function is used for one prototype, since an inline cache that has given up does not flatten anything.
function get1(object) { return object.value; }
function get2(object) { return object.value; }
function get3(object) { return object.value; }
function get4(object) { return object.value; }
function get5(object) { return object.value; }
function get6(object) { return object.value; }
noInline(get1);
noInline(get2);
noInline(get3);
noInline(get4);
noInline(get5);
noInline(get6);

function read(get, prototype) {
    let object = Object.create(prototype);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(get(object), 42);
}

// Returns an object that has spent its one flattening, as if it was read while it was being filled.
function createFlattened() {
    let object = $vm.toCacheableDictionary({ hole: 0, value: 42 });
    $vm.flattenDictionaryObject(object);
    return object;
}

// It is a dictionary again when it becomes a prototype.
let prototype1 = $vm.toCacheableDictionary(createFlattened());
read(get1, prototype1);
shouldBe(isDictionary(prototype1), false);

// It becomes a prototype first. Flattening moves |value| because of the hole.
let prototype2 = createFlattened();
Object.create(prototype2);
$vm.toUncacheableDictionary(prototype2);
delete prototype2.hole;
read(get2, prototype2);
shouldBe(isDictionary(prototype2), false);

// A prototype can be flattened three more times, and then it stays a dictionary.
for (let get of [get3, get4, get5]) {
    $vm.toCacheableDictionary(prototype1);
    read(get, prototype1);
    shouldBe(isDictionary(prototype1), false);
}
$vm.toCacheableDictionary(prototype1);
read(get6, prototype1);
shouldBe(isDictionary(prototype1), true);
