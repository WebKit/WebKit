function bar() {
    return 42.5;
}
noInline(bar);

function baz(value) {
    if (value != 42.5)
        throw "Error: bad value: " + value;
}
noInline(baz);

var True = true;
function foo(a) {
    var x = bar();
    var tmp = 0;
    if (True) {
        var tmp2 = x;
        tmp = a + 1;
        baz(tmp2);
    }
    return x + 1 + tmp;
}
noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(1);
    if (result != 42.5 + 1 + 1 + 1)
        throw "Error: bad result: " + result;
}

var result = foo(2147483647);
if (result != 42.5 + 1 + 2147483647 + 1)
    throw "Error: bad result at end: " + result;
