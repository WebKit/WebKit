// Errors thrown by another realm's built-in must come from that realm, also at call sites that only know its executable.

const other = createGlobalObject();
const object = { valueOf() { return -1; } };

// Calling fresh closures at the same site makes it a closure call, so only Math.abs's executable is known.
function apply(f, x) { return f(x); }
noInline(apply);

for (let i = 0; i < testLoopCount * 2; ++i) {
    apply(Math.abs, object);
    apply(y => y, i);
}

// Polymorphic calls are only inlined by the FTL, so keep going long enough for it to compile apply.
for (let i = 0; i < testLoopCount * 4; ++i) {
    if (apply(Math.abs, object) !== 1)
        throw new Error("bad result");
    apply(y => y, i);
    if (i % 64 !== 63)
        continue;
    let error = null;
    try {
        apply(other.Math.abs, Symbol());
    } catch (e) {
        error = e;
    }
    if (!(error instanceof other.TypeError))
        throw new Error(`expected a TypeError from the callee's realm at iteration ${i}, got ${error}`);
}
