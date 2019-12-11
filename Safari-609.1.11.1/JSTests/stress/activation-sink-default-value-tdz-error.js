//@ skip if $buildType == "debug" or $architecture == "x86"

"use strict";

var n = 10000000;

function shouldThrowTDZ(func) {
    var hasThrown = false;
    try {
        func();
    } catch(e) {
        if (e.name.indexOf("ReferenceError") !== -1)
            hasThrown = true;
    }
    if (!hasThrown)
        throw new Error("Did not throw TDZ error");
}
noInline(shouldThrowTDZ);

function bar(f) { f(10); }

function foo(b) {
    let result = 0;
    var set = function (x) { result = x; }
    var cap = function() { return tdzPerpetrator; }
    if (b) {
        bar(set);
        return tdzPerpetrator;
    }
    let tdzPerpetrator;
    return result;
}

noInline(bar);
noInline(foo);

for (var i = 0; i < n; i++) {
    var bool = !(i % 100);
    if (bool)
        shouldThrowTDZ(function() { foo(bool); });
    else {
        var result = foo(bool);
        if (result != 0)
            throw "Error: bad result: " + result;
    }
}
