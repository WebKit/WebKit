// Tests for GetByVal IC with undefined/null/true/false constant subscripts.

function getUndefined(o) { return o[undefined]; }
function getTrue(o) { return o[true]; }
function getFalse(o) { return o[false]; }
function getNull(o) { return o[null]; }

// Load cases - direct own property
let obj = { undefined: 1, true: 2, false: 3, null: 4 };
for (let i = 0; i < 1e5; i++) {
    if (getUndefined(obj) !== 1) throw new Error("getUndefined failed");
    if (getTrue(obj) !== 2) throw new Error("getTrue failed");
    if (getFalse(obj) !== 3) throw new Error("getFalse failed");
    if (getNull(obj) !== 4) throw new Error("getNull failed");
}

// Miss cases
let obj2 = { x: 1 };
for (let i = 0; i < 1e5; i++) {
    if (getUndefined(obj2) !== undefined) throw new Error("miss undefined failed");
    if (getTrue(obj2) !== undefined) throw new Error("miss true failed");
    if (getFalse(obj2) !== undefined) throw new Error("miss false failed");
    if (getNull(obj2) !== undefined) throw new Error("miss null failed");
}

// Polymorphic - different structures
let obj3 = { undefined: 10, y: 2 };
if (getUndefined(obj3) !== 10) throw new Error("poly undefined failed");

let obj4 = { true: 20, z: 3 };
if (getTrue(obj4) !== 20) throw new Error("poly true failed");

// Prototype chain
function Proto() {}
Proto.prototype["undefined"] = 42;
Proto.prototype["true"] = 43;
Proto.prototype["false"] = 44;
Proto.prototype["null"] = 45;
let obj5 = new Proto();
for (let i = 0; i < 1e5; i++) {
    if (getUndefined(obj5) !== 42) throw new Error("proto undefined failed");
    if (getTrue(obj5) !== 43) throw new Error("proto true failed");
    if (getFalse(obj5) !== 44) throw new Error("proto false failed");
    if (getNull(obj5) !== 45) throw new Error("proto null failed");
}

// Mixed: own property overrides prototype
let obj6 = new Proto();
obj6["undefined"] = 100;
if (getUndefined(obj6) !== 100) throw new Error("override undefined failed");
