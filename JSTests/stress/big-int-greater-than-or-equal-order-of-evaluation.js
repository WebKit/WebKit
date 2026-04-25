function assert(v, e, m) {
    if (v !== e)
        throw new Error(m);
}

let o = {
    [Symbol.toPrimitive]: function() {
        throw new Error("Calling @toPrimitive");
    }
}

try {
    o >= Symbol(2);
    assert(true, false, "")
} catch(e) {
    assert(e.message, "Calling @toPrimitive", "Bad Exception when object is left operand");
}

try {
    Symbol(2) >= o;
    assert(true, false, "")
} catch(e) {
    assert(e instanceof TypeError, true, "Bad Exception when Symbol is left operand");
}

o = {
    [Symbol.toPrimitive]: function() {
        return 2n;
    },

    toString: function() {
        throw new Error("Should never call toString");
    },

    valueOf: function() {
        throw new Error("Should never call valueOf");
    }
}

assert(o >= 3n, false, "ToPrimitive(2n) > 3n");

o = {
    toString: function() {
        throw new Error("Should never call toString");
    },

    valueOf: function() {
        return 2n;
    }
}

assert(o >= 3n, false, "valueOf(2n) > 3n");

