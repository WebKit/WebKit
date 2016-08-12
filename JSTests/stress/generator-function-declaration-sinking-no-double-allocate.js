function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
var GeneratorFunctionPrototype = (function*(){}).__proto__;

function call(o) { o.x = 3; }
noInline(call);

function sink (p, q) {
    function *f() { };
    if (p) {
        call(f); // Force allocation of f
        if (q) {
            OSRExit();
        }
        return f;
    }
    return { 'x': 2 };
}
noInline(sink);

for (var i = 0; i < 100000; ++i) {
    var o = sink(true, false);
    shouldBe(o.__proto__, GeneratorFunctionPrototype);
    if (o.x != 3)
        throw "Error: expected o.x to be 2 but is " + result;
}

// At this point, the function should be compiled down to the FTL

// Check that the function is properly allocated on OSR exit
var f = sink(true, true);
shouldBe(f.__proto__, GeneratorFunctionPrototype);
if (f.x != 3)
    throw "Error: expected o.x to be 3 but is " + result;
