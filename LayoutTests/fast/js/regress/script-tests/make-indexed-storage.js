function foo(length) {
    var o = {};
    o.length = length;
    for (var i = 0; i < o.length; ++i)
        o[i] = i; // The first iteration is special as it makes indexed storage. If the DFG doesn't know how to optimize that and instead calls a C function, then this benchmark may not run so quickly.
    return o;
}

function sum(o) {
    var result = 0;
    for (var i = 0; i < o.length; ++i)
        result += o[i];
    return result;
}

var result = 0;
for (var i = 0; i < 2000; ++i)
    result += sum(foo(100));

if (result != 9900000)
    throw "Error: bad result: " + result;

