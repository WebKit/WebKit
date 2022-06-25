function foo(o, p) {
    var x = 100;
    var result = 101;
    x = 102;
    p = 103;
    result = o.f;
    o = 104;
    p = 105;
    x = 106;
    return {outcome: "return", values: [o, p, x, result]};
}

noInline(foo);

// Warm up foo() with polymorphic objects and non-object types.
for (var i = 0; i < 100000; ++i) {
    var o;
    var isObject = i & 1;
    if (isObject) {
        o = {f:107};
        o["i" + i] = i; // Make it polymorphic.
    } else
        o = 42;
    var result = foo(o);
    if (result.outcome !== "return")
        throw "Error in loop: bad outcome: " + result.outcome;
    if (result.values.length !== 4)
        throw "Error in loop: bad number of values: " + result.values.length;
    if (result.values[0] !== 104)
        throw "Error in loop: bad values[0]: " + result.values[0];
    if (result.values[1] !== 105)
        throw "Error in loop: bad values[1]: " + result.values[1];
    if (result.values[2] !== 106)
        throw "Error in loop: bad values[2]: " + result.values[2];
    if (isObject) {
        if (result.values[3] !== 107)
            throw "Error in loop: bad values[3]: " + result.values[3];
    } else {
        if (result.values[3] !== void 0)
            throw "Error in loop: bad values[3]: " + result.values[3];
    }
}

// Now throw an exception.
var result;
try {
    result = foo(null, 108);
} catch (e) {
    result = {outcome: "exception"};
}
if (result.outcome !== "exception")
    throw "Error at end: bad outcome: " + result.outcome;
