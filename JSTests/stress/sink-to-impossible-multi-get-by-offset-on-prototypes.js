"use strict";

function ThingA() {
}

ThingA.prototype = {bug: 42};

function ThingB() {
}

ThingB.prototype = {bug: 43};

function ThingC() {
}

ThingC.prototype = {bug: 44};

function bar(o, p) {
    if (p)
        return o.bug;
    return null;
}

function foo(p) {
    var o = new ThingC();
    return bar(o, p);
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    bar(new ThingA(), true);
    bar(new ThingB(), true);
}

for (var i = 0; i < 10000; ++i)
    foo(false);

var result = foo(true);
if (result != 44)
    throw new Error("Bad result: " + result);
