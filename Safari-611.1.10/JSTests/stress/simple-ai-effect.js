var doEffect = false;
var didEffect = false;

function bar(o, p)
{
    if (doEffect) {
        delete p.g;
        p.__defineGetter__("g", () => {
            didEffect = true;
            return 42;
        });
    }
}

noInline(bar);

function foo(o, p) {
    var result = o.f + p.g;
    bar(o, p);
    return result + o.f + p.g;
}

noInline(foo);

var o = {g: 1};
o.h = 2;

for (var i = 0; i < 10000; ++i) {
    var result = foo({f: 1}, {g: 3});
    if (result != 8)
        throw "Error: bad result in loop: " + result;
}

doEffect = true;
var result = foo({f: 1}, {g: 3});
if (result != 47)
    throw "Error: bad result at end: " + result;
if (!didEffect)
    throw "Error: did not do effect";
