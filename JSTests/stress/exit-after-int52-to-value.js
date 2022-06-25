function foo(a, b, c) {
    c.f.f = a.f + b.f;
}

noInline(foo);

var counter = 0;
function makeWeirdObject() {
    var result = {};
    result["blah" + (counter++)] = 42;
    return result;
}

for (var i = 0; i < 100000; ++i) {
    var o = makeWeirdObject();
    foo({f:2000000000}, {f:2000000000}, {f:o});
    if (o.f != 4000000000)
        throw "Error: bad result: " + result;
}

var thingy;
Number.prototype.__defineSetter__("f", function(value) { thingy = value; });
foo({f:2000000000}, {f:2000000000}, {f:42});
if (thingy != 4000000000)
    throw "Error: bad result: " + thingy;
