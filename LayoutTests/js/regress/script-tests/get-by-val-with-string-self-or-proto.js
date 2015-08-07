var f = 'f';

function foo(o) {
    return o[f];
}

function bar(a) {
    var result = 0;
    for (var i = 0; i < 2000000; ++i) {
        for (var j = 0; j < a.length; ++j)
            result += foo(a[j]);
    }
    return result;
}

function Foo() {
}

Foo.prototype[f] = 42;

var result = bar([{[f]:24}, new Foo()]);

if (result != 132000000)
    throw "Error bad result: " + result;
