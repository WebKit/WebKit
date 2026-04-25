function assert(b) {
    if (!b)
        throw new Error("Bad assertion.");
}

let x = (new Set)[Symbol.iterator]();
assert(x[Symbol.toStringTag] === "Set Iterator");

let y = {__proto__: x};
assert(y[Symbol.toStringTag] === "Set Iterator");
y[Symbol.toStringTag] = 25;
assert(y[Symbol.toStringTag] === "Set Iterator");
assert(x[Symbol.toStringTag] === "Set Iterator");
