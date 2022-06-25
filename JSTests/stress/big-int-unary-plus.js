function assert(a) {
    if (!a)
        throw new Error("Bad!")
}

function assertTypeError(input) {
    try {
        let a = +input;
        assert(false);
    } catch(e) {
        assert(e instanceof TypeError);
    }
}

assertTypeError(10n);
assertTypeError(-10n);
assertTypeError(Object(10n));
assertTypeError(Object(-10n));

let obj = {
    [Symbol.toPrimitive]: function() {
        return 1n;
    },
    valueOf: function() {
        throw new Error("Should never be called");
    },
    toString: function() {
        throw new Error("Should never be called");
    }
};
assertTypeError(obj);

obj = {
    valueOf: function() {
        return 1n;
    },
    toString: function() {
        throw new Error("Should never be called");
    }
};
assertTypeError(obj);

obj = {
    toString: function() {
        return 1n;
    }
};
assertTypeError(obj);

