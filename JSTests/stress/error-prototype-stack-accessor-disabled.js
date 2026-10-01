//@ requireOptions("--useErrorPrototypeStackAccessor=false")
// FIXME: Remove this test when deleting the useErrorPrototypeStackAccessor JSC Option.
"use strict";

function assert(b, msg = "") {
    if (!b)
        throw new Error("Bad assertion: " + msg);
}

// Legacy: stack is an own DontEnum data property installed on first access.
let e = new Error("msg");
void e.stack; // trigger materialization
let desc = Object.getOwnPropertyDescriptor(e, "stack");
assert(desc !== undefined, "own stack descriptor exists");
assert("value" in desc, "is a data property");
assert(desc.writable === true);
assert(desc.enumerable === false);
assert(desc.configurable === true);

// Legacy: Error.prototype has no "stack".
assert(Object.getOwnPropertyDescriptor(Error.prototype, "stack") === undefined,
    "no accessor on Error.prototype when option is off");

// Legacy: setting non-string is allowed (own property write, no type check).
e.stack = 999;
assert(e.stack === 999);
