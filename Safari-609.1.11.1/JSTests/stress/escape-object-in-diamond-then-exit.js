var global = null;

function foo(p, q) {
    var o = {f:42};
    if (p)
        global = o;
    var tmp = q + 1;
    return o.f + tmp;
}

noInline(foo);

var lastObject = null;

function validateEscape(when) {
    if (global === lastObject)
        throw "Error: bad value in global " + when + ", identical to lastObject.";
    if (global === null || !(typeof global == "object"))
        throw "Error: bad value in global " + when + ": it's not an object.";
    if (global.f != 42)
        throw "Error: bad value in global " + when + ": f isn't 42, it's: " + global.f;
    lastObject = global;
    global = null;
}

for (var i = 0; i < 10000; ++i) {
    var escape = !!(i & 1);
    var result = foo(escape, 42);
    if (result != 42 + 42 + 1)
        throw "Error: bad result: " + result;
    if (escape)
        validateEscape("in loop");
    else if (global !== null)
        throw "Error: bad value in global: " + global;
}

var result = foo(true, 2147483647);
if (result != 42 + 2147483647 + 1)
    throw "Error: bad result at end: " + result;
validateEscape("at end");
