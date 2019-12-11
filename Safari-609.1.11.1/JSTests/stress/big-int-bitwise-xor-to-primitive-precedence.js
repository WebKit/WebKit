//@ runBigIntEnabled

assert = {
    sameValue: function (input, expected, message) {
        if (input !== expected)
            throw new Error(message);
    }
};

let o = {
    [Symbol.toPrimitive]: function() {
        throw new Error("Bad");
    }
};

try{
    o ^ Symbol("2");
    assert.sameValue(true, false, "Exception expected to be throwed, but executed without error");
} catch (e) {
    assert.sameValue(e.message, "Bad", "Expected to throw Error('Bad'), but got: " + e);
}

try{
    Symbol("2") ^ o;
    assert.sameValue(true, false, "Exception expected to be throwed, but executed without error");
} catch (e) {
    assert.sameValue(e instanceof TypeError, true, "Expected to throw TypeError, but got: " + e)
}

