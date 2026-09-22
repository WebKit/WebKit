//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Writers that reach putDirectOffset with no field-type hook, exercised so their safety is checked rather than
// assumed: a poly-proto object's prototype slot at knownPolyProtoOffset, and the fixed-structure VM objects
// (iterator results, RegExp match arrays, property descriptors). The argument is that no creation hook ever
// records those fields, so no claim can exist to violate -- and their values vary in structure, so any claim that
// did form would be false almost immediately.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

// Poly-proto: a constructor defined inside a factory, so each call site gets its own prototype.
function makeClass(tag) {
    class C {
        constructor(v) { this.field = v; }
        get() { return this.field; }
    }
    C.prototype.tag = tag;
    return C;
}

function Payload(a) { this.a = a; }
function readField(o) { return o.field.a; }
function readTag(o) { return o.tag; }
noInline(readField);
noInline(readTag);

const classes = [];
for (let i = 0; i < 8; ++i)
    classes.push(makeClass(i));

const objs = [];
for (let i = 0; i < 64; ++i) {
    const C = classes[i & 7];
    objs.push(new C(new Payload(i)));
}
gc();
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    shouldBe(readField(objs[idx]), idx);
    shouldBe(readTag(objs[idx]), idx & 7);
}

// Contradict the field on half of them, so any claim on `field` must have been withdrawn.
function OtherPayload(a) { this.zz = 0; this.a = a * 2; }
for (let i = 0; i < 32; ++i)
    objs[i].field = new OtherPayload(i);
gc();
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i & 63;
    shouldBe(readField(objs[idx]), idx < 32 ? idx * 2 : idx);
}

// Iterator results, whose `value` holds whatever the iterator yields.
function* gen(kind, n) {
    for (let i = 0; i < n; ++i) {
        if (kind === 0) yield new Payload(i);
        else if (kind === 1) yield new OtherPayload(i);
        else yield i;
    }
}
function drain(kind) {
    let seen = 0;
    for (const v of gen(kind, 32)) {
        if (kind === 0) shouldBe(v.a, seen);
        else if (kind === 1) shouldBe(v.a, seen * 2);
        else shouldBe(v, seen);
        ++seen;
    }
    return seen;
}
for (let round = 0; round < 64; ++round)
    shouldBe(drain(round % 3), 32);
gc();

// The manual iterator protocol, producing { value, done } objects through the same fixed structure.
function manualDrain(kind) {
    const it = gen(kind, 16)[Symbol.iterator]();
    let seen = 0;
    for (;;) {
        const r = it.next();
        if (r.done)
            break;
        if (kind === 0) shouldBe(r.value.a, seen);
        ++seen;
    }
    return seen;
}
for (let round = 0; round < 64; ++round)
    shouldBe(manualDrain(round % 3), 16);

// RegExp match arrays: `groups` and `index` are written by putDirectOffset at fixed offsets.
const re = /(?<word>[a-z]+)(\d+)/;
for (let i = 0; i < testLoopCount / 4; ++i) {
    const m = re.exec("abc" + (i % 10));
    shouldBe(m.groups.word, "abc");
    shouldBe(m.index, 0);
    shouldBe(m[2], String(i % 10));
}

// Property descriptors, likewise fixed-structure, with values of varying shape.
const src = {};
Object.defineProperty(src, "p", { value: new Payload(1), writable: true, enumerable: true, configurable: true });
for (let i = 0; i < 2000; ++i) {
    const d = Object.getOwnPropertyDescriptor(src, "p");
    shouldBe(d.value.a, 1);
    shouldBe(d.writable, true);
}
Object.defineProperty(src, "p", { value: new OtherPayload(3) });
for (let i = 0; i < 2000; ++i)
    shouldBe(Object.getOwnPropertyDescriptor(src, "p").value.a, 6);
gc();
shouldBe(src.p.a, 6);
