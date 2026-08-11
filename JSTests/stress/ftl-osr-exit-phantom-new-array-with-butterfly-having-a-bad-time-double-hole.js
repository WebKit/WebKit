//@ runDefault("--jitPolicyScale=0.1")

let trigger = false;
let getterCalls = 0;
let setterCalls = 0;

function cb() {
    if (trigger) {
        Object.defineProperty(Array.prototype, 3, {
            get() { getterCalls++; return 42; },
            set(value) { setterCalls++; },
            configurable: true
        });
    }
}
noInline(cb);

function collect() { gc(); }
noInline(collect);

function opt(escape) {
    let a = new Array(5);
    a[0] = 1.1;
    a[1] = 2.2;
    a[2] = 3.3;
    a[4] = 5.5;
    cb();
    collect();
    // Index 3 is written only after the cb() call so that the OSR exit triggered by having a bad
    // time during cb() rematerializes the sunk double Array while index 3 still holds the hole
    // default (PNaN).
    a[3] = 4.4;
    if (escape)
        return a;
    return 0;
}
noInline(opt);

for (let i = 0; i < 1000; i++)
    opt(!(i % 10));

trigger = true;
let a = opt(true);

// After having a bad time, the rematerialized Array uses SlowPutArrayStorage and index 3 must be
// a hole at the OSR exit. The subsequent a[3] = 4.4 store in baseline code must therefore be
// forwarded to the Array.prototype setter instead of creating an own property.
if (a.hasOwnProperty(3))
    throw new Error("index 3 should not be an own property, got value: " + a[3]);
if (a[3] !== 42)
    throw new Error("a[3] should return 42 from the prototype getter, got: " + a[3]);
if (setterCalls !== 1)
    throw new Error("Array.prototype setter should have been called once, got: " + setterCalls);
if (a[0] !== 1.1 || a[1] !== 2.2 || a[2] !== 3.3 || a[4] !== 5.5)
    throw new Error("other elements corrupted: " + a[0] + "," + a[1] + "," + a[2] + "," + a[4]);

// The rematerialized ArrayStorage must also have an accurate m_numValuesInVector so that
// hasHoles() is true. Otherwise Array.prototype.reverse takes a fast path that moves the raw
// hole around without consulting the prototype accessors.
let getterCallsBeforeReverse = getterCalls;
let setterCallsBeforeReverse = setterCalls;
a.reverse();
if (getterCalls === getterCallsBeforeReverse || setterCalls === setterCallsBeforeReverse)
    throw new Error("reverse should have consulted the prototype accessors (getter calls: "
        + (getterCalls - getterCallsBeforeReverse) + ", setter calls: " + (setterCalls - setterCallsBeforeReverse) + ")");
if (a[0] !== 5.5 || a[1] !== 42 || a[2] !== 3.3 || a[4] !== 1.1)
    throw new Error("reverse produced wrong elements: " + a[0] + "," + a[1] + "," + a[2] + "," + a[4]);
if (a.hasOwnProperty(3) || a[3] !== 42)
    throw new Error("after reverse, index 3 should still be a hole reading 42 from the getter, got: " + a[3]);
gc();
