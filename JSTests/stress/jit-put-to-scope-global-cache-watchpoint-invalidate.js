function foo(p, v) {
    if (p)
        global = v;
}

function bar() {
    return global;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 10; ++i)
    foo(false);

var value = 42;
foo(true, value);
var n = 100000;
var m = 100;
for (var i = 0; i < n; ++i) {
    if (i == n - m)
        foo(true, value = 53);
    var result = bar();
    if (result != value)
        throw "Error: on iteration " + i + " got: " + result;
}
