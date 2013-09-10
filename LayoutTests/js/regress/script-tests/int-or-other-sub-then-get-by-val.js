var p = {f:true};

function foo(a, b, d) {
    var c;
    if (p.f)
        c = b;
    return d[a - c];
}

var result = 0;
var array = [42, 43, 44];
for (var i = 0; i < 1000000; ++i)
    result += foo(i + 1, i - 1, array);

if (result != 44000000)
    throw "Bad result: " + result;

