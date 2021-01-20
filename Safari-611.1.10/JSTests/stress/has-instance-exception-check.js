function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

let getter = Object.getOwnPropertyDescriptor({get foo(){}}, "foo").get;
Object.defineProperty(getter, Symbol.hasInstance, {value:undefined});
let y = {};
Object.defineProperty(getter, "prototype", {get: Uint8Array});
let error = null;
try {
    y instanceof getter;
} catch(e) {
    error = e;
}
assert(!!error);
assert(error.toString() === "TypeError: calling Uint8Array constructor without new is invalid");
