var o = {};

function foo(o) {
    var result = 0;
    for (var i = 0; i < o.length; ++i)
        result += o[i];
    return result;
}

o.length = 100;
for (var i = 0; i < o.length; ++i)
    o[i] = i;

var result = 0;
for (var i = 0; i < 10000; ++i)
    result += foo(o);

if (result != 49500000)
    throw "Error: bad result " + result;
