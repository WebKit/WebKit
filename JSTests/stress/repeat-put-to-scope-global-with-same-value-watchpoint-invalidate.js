function foo(v) {
    global = v;
}

function bar() {
    return global;
}

noInline(foo);
noInline(bar);

var value = 42;
for (var i = 0; i < 10; ++i)
    foo(value);
var n = 100000;
var m = 100;
for (var i = 0; i < n; ++i) {
    if (i == n - m)
        foo(value = 53);
    var result = bar();
    if (result != value)
        throw "Error: on iteration " + i + " got: " + result;
}
