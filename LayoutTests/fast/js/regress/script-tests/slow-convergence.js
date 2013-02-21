function foo(a, b, c, d) {
    var result = a;
    if (a < 0)
        throw "what!";
    else if (a < 1) {
        for (var i = b; i < c; ++i)
            result += d[i];
    } else if (a < 2) {
        for (var i = b + 1; i < c - 1; ++i)
            result += d[i] * a;
    } else if (a < 3) {
        for (var i = b + 2; i < c - 2; ++i)
            result += d[i] * b;
    } else if (a < 4) {
        for (var i = b + 3; i < c - 3; ++i)
            result += d[i] * c;
    } else
        throw "huh?";
    return result;
}

var array = [];
for (var i = 0; i < 20; ++i)
    array.push(i);

var limit = 20000;
var phases = 4;
var result = 0;
for (var i = 0; i < limit; ++i) {
    var phase = (i * phases / limit) | 0;
    result += foo(i % (phase + 1), ((i % array.length) / 2) | 0, array.length - (((i % array.length) / 2) | 0), array);
}

if (result != 3072367)
    throw "Bad result: " + result;


