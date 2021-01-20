function foo(a, b) {
    return a + b;
}

function verify(a, b) {
    if (a !== b)
        throw "Error: the two arguments objects aren't identical.";
}

noInline(verify);

function bar() {
    var a = arguments;
    this.verify(arguments, a);
    return foo.apply(null, a);
}

function baz(a, b) {
    return this.bar(a + 1, b + 1);
}

noInline(baz);

for (var i = 0; i < 20000; ++i) {
    var o = {
        baz: baz,
        bar: bar,
        verify: function() { }
    };
    var result = o.baz(1, 2);
    if (result != 1 + 1 + 2 + 1)
        throw "Error: bad result: " + result;
}

var o = {
    baz: baz,
    bar: bar,
    verify: verify
};
var result = o.baz(1, 2);
if (result != 1 + 1 + 2 + 1)
    throw "Error: bad result at end: " + result;
