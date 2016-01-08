// This failure will be resolved by https://bugs.webkit.org/show_bug.cgi?id=150279.
//@ skip

function foo(o, p) {
    var x = 100;
    var result = 101;
    var pf = p.g;
    try {
        x = 102;
        pf++;
        o.f = x + pf;
        o = 104;
        pf++;
        x = 106;
    } catch (e) {
        return {outcome: "exception", values: [o, pf, x]};
    }
    return {outcome: "return", values: [o, pf, x]};
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
    var result = foo(o, {g:200});
    if (result.outcome !== "return")
        throw "Error in loop: bad outcome: " + result.outcome;
    if (result.values.length !== 3)
        throw "Error in loop: bad number of values: " + result.values.length;
    if (result.values[0] !== 104)
        throw "Error in loop: bad values[0]: " + result.values[0];
    if (result.values[1] !== 202)
        throw "Error in loop: bad values[1]: " + result.values[1];
    if (result.values[2] !== 106)
        throw "Error in loop: bad values[2]: " + result.values[2];
    if (o._f != 102 + 201)
        throw "Error in loop: bad value of o._f: " + o._f;
}

// Now throw an exception.
var o = {};
o.__defineSetter__("f", function() {
    throw "Error42";
});
var result = foo(o, {g:300});
if (result.outcome !== "exception")
    throw "Error at end: bad outcome: " + result.outcome;
if (result.values.length !== 3)
    throw "Error at end: bad number of values: " + result.values.length;
if (result.values[0] !== o)
    throw "Error at end: bad values[0]: " + result.values[0];
if (result.values[1] !== 301)
    throw "Error at end: bad values[1]: " + result.values[1];
if (result.values[2] !== 102)
    throw "Error at end: bad values[2]: " + result.values[2];
if ("_f" in o)
    throw "Error at end: o has _f.";

