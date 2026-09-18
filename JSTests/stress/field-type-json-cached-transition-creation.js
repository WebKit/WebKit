//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// The JSON parser's cached-transition creation path. Once LiteralParser has seen a (structure, offset)
// transition it builds later same-shaped objects with putDirectOffset + setStructure directly, so that path needs
// its own field-type hook: the first object of a shape establishes a claim through the slow branch, and without
// the hook no later object of that shape ever withdraws it.

// No store is involved, so store-side diagnostics see nothing -- at creation the record's owner is a descendant
// of the object's current structure, and the store-side walk only visits ancestors.

// One shape whose fields hold values of differing structures. Element 0 establishes the claim through the slow
// branch; the rest come through the cached transition. `min` alternates int-only against double-containing arrays,
// which differ in indexing type and therefore in structure.
function makeDocument() {
    const parts = [];
    for (let i = 0; i < 48; ++i) {
        if (i % 3 === 0)
            parts.push('{"pbr":{"a":1},"min":[0,0,0]}');
        else if (i % 3 === 1)
            parts.push('{"pbr":{"b":2},"min":[0.5,1.5,2.5]}');
        else
            parts.push('{"pbr":{"a":1,"b":2},"min":[1,2,3]}');
    }
    return "[" + parts.join(",") + "]";
}

const text = makeDocument();
const parsed = JSON.parse(text);
if (parsed.length !== 48)
    throw new Error("unexpected length " + parsed.length);

// A stale claim folds the CheckStructure on the loaded value.
function readPbrA(o) { return o.pbr.a; }
function readPbrB(o) { return o.pbr.b; }
function readMin0(o) { return o.min[0]; }
noInline(readPbrA);
noInline(readPbrB);
noInline(readMin0);

for (let i = 0; i < testLoopCount; ++i) {
    const o = parsed[i % parsed.length];
    switch ((i % parsed.length) % 3) {
    case 0:
        if (readPbrA(o) !== 1)
            throw new Error("expected pbr.a === 1, got " + String(readPbrA(o)));
        if (readPbrB(o) !== undefined)
            throw new Error("expected pbr.b === undefined, got " + String(readPbrB(o)));
        if (readMin0(o) !== 0)
            throw new Error("expected min[0] === 0, got " + String(readMin0(o)));
        break;
    case 1:
        if (readPbrA(o) !== undefined)
            throw new Error("expected pbr.a === undefined, got " + String(readPbrA(o)));
        if (readPbrB(o) !== 2)
            throw new Error("expected pbr.b === 2, got " + String(readPbrB(o)));
        if (readMin0(o) !== 0.5)
            throw new Error("expected min[0] === 0.5, got " + String(readMin0(o)));
        break;
    default:
        if (readPbrA(o) !== 1 || readPbrB(o) !== 2)
            throw new Error("expected pbr {a:1,b:2}, got a=" + String(readPbrA(o)) + " b=" + String(readPbrB(o)));
        if (readMin0(o) !== 1)
            throw new Error("expected min[0] === 1, got " + String(readMin0(o)));
        break;
    }
}

const again = JSON.parse(text);
for (let i = 0; i < again.length; ++i) {
    const o = again[i];
    if (typeof o.pbr !== "object" || o.pbr === null)
        throw new Error("pbr is not an object at " + i);
    if (o.min.length !== 3)
        throw new Error("min length wrong at " + i);
}
gc();
for (let i = 0; i < testLoopCount; ++i) {
    const o = again[i % again.length];
    if (readMin0(o) !== parsed[i % again.length].min[0])
        throw new Error("min[0] mismatch between documents at " + (i % again.length));
}
