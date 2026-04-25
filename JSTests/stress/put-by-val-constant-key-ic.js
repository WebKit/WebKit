// Tests for PutByVal IC with undefined/null/true/false constant subscripts.

function putUndefined(o, v) { o[undefined] = v; }
function putTrue(o, v) { o[true] = v; }
function putFalse(o, v) { o[false] = v; }
function putNull(o, v) { o[null] = v; }

// Replace cases - property already exists
for (let i = 0; i < 1e5; i++) {
    let obj = { undefined: 0, true: 0, false: 0, null: 0 };
    putUndefined(obj, 1);
    putTrue(obj, 2);
    putFalse(obj, 3);
    putNull(obj, 4);
    if (obj["undefined"] !== 1) throw new Error("replace undefined failed: " + obj["undefined"]);
    if (obj["true"] !== 2) throw new Error("replace true failed: " + obj["true"]);
    if (obj["false"] !== 3) throw new Error("replace false failed: " + obj["false"]);
    if (obj["null"] !== 4) throw new Error("replace null failed: " + obj["null"]);
}

// Transition cases - property doesn't exist yet, structure changes
function putUndefined2(o, v) { o[undefined] = v; }
function putTrue2(o, v) { o[true] = v; }
function putFalse2(o, v) { o[false] = v; }
function putNull2(o, v) { o[null] = v; }

for (let i = 0; i < 1e5; i++) {
    let obj = { x: 1 };
    putUndefined2(obj, 10);
    if (obj["undefined"] !== 10) throw new Error("transition undefined failed");

    let obj2 = { x: 1 };
    putTrue2(obj2, 20);
    if (obj2["true"] !== 20) throw new Error("transition true failed");

    let obj3 = { x: 1 };
    putFalse2(obj3, 30);
    if (obj3["false"] !== 30) throw new Error("transition false failed");

    let obj4 = { x: 1 };
    putNull2(obj4, 40);
    if (obj4["null"] !== 40) throw new Error("transition null failed");
}

// Polymorphic structures - different initial shapes
function putUndefined3(o, v) { o[undefined] = v; }
for (let i = 0; i < 1e5; i++) {
    let obj;
    if (i % 3 === 0)
        obj = { a: 1 };
    else if (i % 3 === 1)
        obj = { b: 2 };
    else
        obj = { c: 3 };
    putUndefined3(obj, i);
    if (obj["undefined"] !== i) throw new Error("polymorphic undefined failed at " + i);
}

// Verify that regular property access still works
for (let i = 0; i < 1e5; i++) {
    let obj = { undefined: 1, true: 2, false: 3, null: 4, x: 5 };
    putUndefined(obj, 100);
    if (obj.x !== 5) throw new Error("other property corrupted");
    if (obj["undefined"] !== 100) throw new Error("re-replace failed");
}

// Replace with different value types
function putUndefined4(o, v) { o[undefined] = v; }
for (let i = 0; i < 1e5; i++) {
    let obj = { undefined: 0 };
    putUndefined4(obj, i);
    if (obj["undefined"] !== i) throw new Error("value type int failed");
    putUndefined4(obj, "str");
    if (obj["undefined"] !== "str") throw new Error("value type string failed");
    putUndefined4(obj, null);
    if (obj["undefined"] !== null) throw new Error("value type null failed");
}
