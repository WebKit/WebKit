function assert(v, e, m) {
    if (v !== e)
        throw new Error(m);
}

assert(Object(2n) < 1n, false, "Object(2n) < 1n");
assert(1n < Object(2n), true, "1n < Object(2n)");
assert(Object(2n) < Object(1n), false, "Object(2n) < Object(1n)");
assert(Object(1n) < Object(2n), true, "Object(1n) < Object(2n)");

let o = {
    [Symbol.toPrimitive]: function() {
        return 2n;
    }
}

let o2 = {
    [Symbol.toPrimitive]: function() {
        return 1n;
    }
}

assert(o < 1n, false, "ToPrimitive(2n) < 1n");
assert(1n < o, true, "1n < ToPrimitive(2n)");
assert(o < o2, false, "ToPrimitive(2n) < ToPrimitive(1n)");
assert(o2 < o, true, "ToPrimitive(1n) < ToPrimitive(2n)");

o = {
    valueOf: function() {
        return 2n;
    }
}

o2 = {
    valueOf: function() {
        return 1n;
    }
}

assert(o < 1n, false, "ToPrimitive(2n) < 1n");
assert(1n < o, true, "1n < ToPrimitive(2n)");
assert(o < o2, false, "ToPrimitive(2n) < ToPrimitive(1n)");
assert(o2 < o, true, "ToPrimitive(1n) < ToPrimitive(2n)");

o = {
    toString: function() {
        return 2n;
    }
}

o2 = {
    toString: function() {
        return 1n;
    }
}

assert(o < 1n, false, "ToPrimitive(2n) < 1n");
assert(1n < o, true, "1n < ToPrimitive(2n)");
assert(o < o2, false, "ToPrimitive(2n) < ToPrimitive(1n)");
assert(o2 < o, true, "ToPrimitive(1n) < ToPrimitive(2n)");

