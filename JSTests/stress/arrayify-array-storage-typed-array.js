function make1() {
    var a = [];
    a.__defineGetter__(42, function() { return 42; });
    return a;
}

noInline(make1);

function make2() {
    return [42];
}

noInline(make2);

for (var i = 0; i < 10000; ++i) {
    make1();
    make2();
}

function foo(o) {
    o[0] = 0;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    foo(make1());
    foo(make2());
}

var o = new Int8Array(100);
var b = o.buffer;
foo(o);
if (o.buffer != b)
    throw "Error: buffer changed.";

