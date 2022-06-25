function ThingA() {
}

ThingA.prototype = {f:1};

function ThingB() {
}

ThingB.prototype = {f:2};

function foo(o, p) {
    return p ? o.f : -1;
}

for (var i = 0; i < 10000; ++i) {
    foo(new ThingA(), true);
    foo(new ThingB(), true);
    ThingA.prototype.f = i;
    ThingB.prototype.f = i + 1;
}

function bar(p) {
    return foo(new ThingA(), p);
}

ThingA.prototype.f = 42;

for (var i = 0; i < 10000; ++i) {
    var result = bar(false);
    if (result != -1)
        throw new Error("Bad result in loop: " + result);
}

var result = bar(true);
if (result != 42)
    throw new Error("Bad result: " + result);


