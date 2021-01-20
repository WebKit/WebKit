function foo(x) {
    return new Array(x);
}

noInline(foo);

function test(size) {
    var result = foo(size);
    if (result.length != size) {
        print("Got a weird length: " + result.length);
        throw "Error: bad result: " + result;
    }
    var sawThings = false;
    for (var s in result)
        sawThings = true;
    if (sawThings) {
        print("Saw things!");
        throw "Error: array is in bad state: " + result;
    }
    result[0] = "42.5";
    if (result[0] != "42.5") {
        print("Didn't store what I thought I stored.");
        throw "Error: array is in wierd state: " + result;
    }
}

for (var i = 0; i < 100000; ++i) {
    test(0);
    test(1);
    test(42);
}
