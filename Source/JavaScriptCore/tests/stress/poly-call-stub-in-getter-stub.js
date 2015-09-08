function foo(o) {
    return o.f;
}

noInline(foo);

var p = {f:42};

var doBadThings = false;
function makeGetter() {
    return function() {
        if (doBadThings) {
            delete p.f;
            fullGC();
            return 43;
        }
        return 42;
    };
}

for (var i = 0; i < 10000; ++i) {
    var o = Object.create(p);
    if (i & 1) {
        o.__defineGetter__("f", makeGetter());
    }

    var result = foo(o);
    if (result != 42)
        throw "Error: bad result: " + result;
}

var o = Object.create(p);
o.__defineGetter__("f", makeGetter());
doBadThings = true;
var result = foo(o);
if (result != 43)
    throw "Error: bad result at end: " + result;
