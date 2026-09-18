//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1", "--useJIT=0")

// A field-type claim that is false the moment it is made: no store is involved. When the first object creates a
// field with a non-cell, recordFieldTypeAtCreation must insert a permanently generalised entry, not merely try to
// generalize() an entry that does not exist yet -- otherwise a later same-shape object creating that field with a
// cell takes the brand-new-entry branch and establishes a claim the first object already contradicts.

// Nothing on the store side can ever see this, which is why store-side diagnostics reported zero violations.

// A store site only leaves a field claimable when it is never cached: a cached put_by_id site poisons the field
// via declineCachingForFieldType. Hence one put_by_val site driven with many distinct keys.
const keys = [];
for (let i = 0; i < 64; ++i)
    keys.push("k" + i + ".5");

function build(k, v) {
    const o = {};
    o[k] = v;
    return o;
}
noInline(build);

const held = [];

// Every key's field is created holding a double, which must leave it permanently generalised.
for (const k of keys) {
    held.push(build(k, 1.5));
    held.push(build(k, 2.5));
}

// Required for teeth: the heap verifier walks live cells, and cells allocated before the run's first collection
// are not yet visible to that walk, so a false claim on the phase-1 objects would go unreported.
gc();

// The same keys, now holding cells. Without the fix each establishes a claim the doubles contradict.
for (const k of keys)
    held.push(build(k, "a string"));

// Catch the claim while it is still live and still false, before anything below can withdraw it.
gc();

// A false claim here turns a double read through optimised code into a wrong value.
function readField(o, k) { return o[k]; }
noInline(readField);

for (let i = 0; i < testLoopCount; ++i) {
    const idx = i % held.length;
    const o = held[idx];
    const k = keys[Math.floor(idx / 2) % keys.length];
    readField(o, k);
}

for (let i = 0; i < keys.length; ++i) {
    const k = keys[i];
    if (held[i * 2][k] !== 1.5)
        throw new Error("expected 1.5 for " + k + ", got " + String(held[i * 2][k]));
    if (held[i * 2 + 1][k] !== 2.5)
        throw new Error("expected 2.5 for " + k + ", got " + String(held[i * 2 + 1][k]));
    if (held[keys.length * 2 + i][k] !== "a string")
        throw new Error("expected the string for " + k + ", got " + String(held[keys.length * 2 + i][k]));
}

// Exercise any surviving claim from both directions.
for (let i = 0; i < keys.length; ++i) {
    held[i * 2][keys[i]] = { tag: i };
    held[keys.length * 2 + i][keys[i]] = i;
}
for (let i = 0; i < testLoopCount; ++i) {
    const idx = i % keys.length;
    const viaObject = held[idx * 2][keys[idx]];
    if (viaObject.tag !== idx)
        throw new Error("expected tag " + idx + ", got " + String(viaObject && viaObject.tag));
    const viaNumber = held[keys.length * 2 + idx][keys[idx]];
    if (viaNumber !== idx)
        throw new Error("expected " + idx + ", got " + String(viaNumber));
}
