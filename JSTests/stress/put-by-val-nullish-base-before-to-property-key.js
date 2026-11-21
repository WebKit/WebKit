//@ runDefault("--useJIT=false")
//@ runNoLLInt("--useConcurrentJIT=false")

// https://bugs.webkit.org/show_bug.cgi?id=332101
// PutValue must reject a nullish base before converting the property key.

function assign(base, key) {
    base[key] = 1;
}
noInline(assign);

function assignStrict(base, key) {
    "use strict";
    base[key] = 1;
}
noInline(assignStrict);

function assignDestructuring(base, key) {
    [base[key]] = [1];
}
noInline(assignDestructuring);

var conversions = 0;
var keys = [
    { toString() { ++conversions; return "key"; } },
    { toString() { ++conversions; throw new RangeError("key"); } },
    { [Symbol.toPrimitive]() { ++conversions; return "key"; } },
    { [Symbol.toPrimitive]() { ++conversions; throw new RangeError("key"); } },
];
var assignments = [assign, assignStrict, assignDestructuring];

for (var i = 0; i < 1e6; ++i) {
    var caught = false;
    try {
        assignments[i % assignments.length](i & 1 ? null : undefined, keys[(i >> 1) % keys.length]);
    } catch (error) {
        if (!(error instanceof TypeError))
            throw new Error("Expected TypeError, got " + error);
        caught = true;
    }
    if (!caught)
        throw new Error("Expected TypeError for a nullish base");
    if (conversions !== 0)
        throw new Error("Property key was converted before rejecting a nullish base");
}
