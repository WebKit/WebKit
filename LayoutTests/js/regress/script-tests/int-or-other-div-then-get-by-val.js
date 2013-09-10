var p = {f:true};

function foo(a, b, d) {
    var c;
    if (p.f)
        c = b;
    return d[a / c];
}

var result = 0;
var array = [42, 43, 44, 45, 46];
for (var i = 0; i < 300000; ++i)
    result += foo((i % 3) * 2, (i % 2) + 1, array);

if (result != 13050000)
    throw "Bad result: " + result;

