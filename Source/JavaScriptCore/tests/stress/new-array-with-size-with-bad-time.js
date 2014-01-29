function foo(x) {
    return new Array(x);
}

noInline(foo);

var poke;
Array.prototype.__defineSetter__("1", function() {
    poke = "poke";
});

function test(size) {
    var result = foo(size);
    if (result.length != size)
        throw "Error: bad result: " + result;
    var sawThings = false;
    for (var s in result) {
        if (s != "1")
            sawThings = true;
    }
    if (sawThings)
        throw "Error: array is in bad state: " + result;
    result[0] = "42.5";
    if (result[0] != "42.5")
        throw "Error: array is in wierd state: " + result;
    poke = null;
    result[1] = 42;
    if (poke != "poke")
        throw "Error: setter not called.";
}

for (var i = 0; i < 100000; ++i) {
    test(42);
}
