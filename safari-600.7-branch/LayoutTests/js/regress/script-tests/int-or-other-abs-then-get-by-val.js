var p = {f:true};

function foo(b, d) {
    var c;
    if (p.f)
        c = b;
    return d[Math.abs(c)];
}

var result = 0;
var array = [42, 43, 44, 45];
for (var i = 0; i < 500000; ++i)
    result += foo(-((i % 3) + 1), array);

if (result != 21999999)
    throw "Bad result: " + result;

