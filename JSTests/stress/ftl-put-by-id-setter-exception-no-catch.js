function foo(o, p) {
    var x = 100;
    var result = 101;
    x = 102;
    p = 103;
    o.f = x + p;
    o = 104;
    p = 105;
    x = 106;
    return {outcome: "return", values: [o, p, x]};
}

noInline(foo);

// Warm up foo() with polymorphic objects and getters.
for (var i = 0; i < 100000; ++i) {
    var o = {};
    o.__defineSetter__("f", function(value) {
        this._f = value;
    });
    if (i & 1)
        o["i" + i] = i; // Make it polymorphic.
    var result = foo(o);
    if (result.outcome !== "return")
        throw "Error in loop: bad outcome: " + result.outcome;
    if (result.values.length !== 3)
        throw "Error in loop: bad number of values: " + result.values.length;
    if (result.values[0] !== 104)
        throw "Error in loop: bad values[0]: " + result.values[0];
    if (result.values[1] !== 105)
        throw "Error in loop: bad values[1]: " + result.values[1];
    if (result.values[2] !== 106)
        throw "Error in loop: bad values[2]: " + result.values[2];
    if (o._f != 102 + 103)
        throw "Error in loop: bad value of o._f: " + o._f;
}

// Now throw an exception.
var result;
try {
    var o = {};
    o.__defineSetter__("f", function() {
        throw "Error42";
    });
    result = foo(o, 108);
} catch (e) {
    if (e != "Error42")
        throw "Error at end: bad exception: " + e;
    result = {outcome: "exception"};
}
if (result.outcome !== "exception")
    throw "Error at end: bad outcome: " + result.outcome;
if ("_f" in o)
    throw "Error at end: o has _f";
