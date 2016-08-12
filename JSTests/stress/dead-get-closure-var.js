var global;

function foo(a) {
    var x = a.f;
    var f = function() { global = x; };
    noInline(f);
    f();
    var tmp1 = a.g + 1;
    var tmp2 = x;
    return global;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:i, g:i + 1});
    if (result != i)
        throw "Error: bad result: " + result;
}

var result = foo({f:42, g:4.2});
if (result != 42)
    throw "Error: bad result at end: " + result;
