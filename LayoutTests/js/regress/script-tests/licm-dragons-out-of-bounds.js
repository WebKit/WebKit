function foo(p, o, i) {
    var result = 0;
    for (var j = 0; j < 1000; ++j) {
        if (p)
            result += o[i];
        else
            result++;
    }
    return result;
}

noInline(foo);

var o = [42];

var result = 0;

for (var i = 0; i < 1000; ++i) {
    result += foo(true, o, 0);
    result += foo(false, o, 1);
}

for (var i = 0; i < 10000; ++i)
    result += foo(false, o, 1);

for (var i = 0; i < 20000; ++i)
    result += foo(true, o, 0);

if (result != 893000000)
    throw "Error: bad result: " + result;
