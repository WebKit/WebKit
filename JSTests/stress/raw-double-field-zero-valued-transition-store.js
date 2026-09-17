// Regression test for a null-deref in the raw-double field representation.
//
// The trap is that bits(0.0) == 0, and JSC's NaN boxing is box(d) = bits(d) + DoubleEncodeOffset.
// So if a raw (unboxed) double slot holding 0.0 is read by a path that expects a boxed JSValue,
// the read yields bits(0.0) - DoubleEncodeOffset ... but if it is read *without* the bias at all it
// yields 0 -- the EMPTY JSValue, which passes isCell() and null-derefs on the next field access.
// The ASSERT(value) that would have caught this is compiled out in release builds, so this
// reproduced as a bare SIGSEGV only in release.
//
// The specific bug: the DFG bytecode parser declined to emit a raw-double store for a Transition
// put whose value was not *proven* numeric, on the theory that the inline cache would handle it.
// At parse time shouldSpeculateNumber() is unpopulated for an inlined call argument, so this
// declined essentially every constructor field store, routing them to an IC that wrote raw bits
// into a slot the C++ readers then read as boxed.
//
// Keep the literal 0.0 arguments: a nonzero double would corrupt the value instead of crashing,
// which is a much harder failure to notice.

function V(x, y, z) {
    this.x = x;
    this.y = y;
    this.z = z;
}

// Do NOT noInline() mk(): the bug requires the `new V(...)` to be INLINED into the loop, because
// the unpopulated prediction it depended on is the one for an inlined call argument. With mk()
// forced out of line the argument prediction is populated and the bug does not reproduce at all.
function mk() {
    return new V(0.0, 0.0, 0.0);
}

var acc = 0;
for (var i = 0; i < 300000; ++i) {
    var v = mk();
    acc += v.x + v.y + v.z;
}

if (acc !== 0)
    throw new Error(`expected 0, got ${acc}`);
