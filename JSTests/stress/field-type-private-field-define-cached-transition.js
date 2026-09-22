//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Private-field define through a cached LLInt transition. slow_path_put_private_name's NewProperty branch caches
// (oldStructureID, offset, newStructureID) in bytecode metadata, after which LLInt stores from asm and nothing
// observes the value, so the site must call declineCachingForFieldType. That helper also has to walk the ancestry
// when the immediate owner cannot be named: returning false there allows caching and reopens the same hole.

// The field needs an INITIALIZER to reach this: `#v;` with no initializer defines with jsUndefined() and a
// non-cell creation is permanently generalised, and `this.#v = v` is a Replace, which is declined already.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Strength() { this.value = 111; }
function Other() { this.a = 1; this.b = 2; this.c = 3; }   // different structure

// The initializer reads a closure variable, so the define is cell-valued and the cell is ours to choose.
let next = null;
class Box {
    #v = next;
    read() { return this.#v; }
}

// Establish (Box-shape, #v) -> Strength and warm the path until the LLInt transition is cached, which is the state
// the bug lives in.
const boxes = [];
for (let i = 0; i < testLoopCount; ++i) {
    next = new Strength();
    const b = new Box();
    if (boxes.length < 256)
        boxes.push(b);
    shouldBe(b.read().value, 111);
}
gc();

// Define with a value that contradicts the claim; the heap walk at the next gc() is the real check.
next = new Other();
const violator = new Box();
shouldBe(violator.read().b, 2);
shouldBe(violator.read().value, undefined);

gc();

// Keep going, so the violating object is alive across more than one collection.
const tail = [];
for (let i = 0; i < 64; ++i) {
    next = (i & 1) ? new Other() : new Strength();
    const b = new Box();
    tail.push(b);
    shouldBe(b.read() === null, false);
}
gc();

// Bounded by what was actually pushed: testLoopCount is small in mini-mode and the eager configurations.
const recorded = Math.min(boxes.length, 256);
for (let i = 0; i < recorded; ++i)
    shouldBe(boxes[i].read().value, 111);
shouldBe(violator.read().b, 2);
