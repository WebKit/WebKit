function foo(length, value) {
    var o = {};
    o.length = length;
    for (var i = 0; i < o.length; ++i)
        o[i] = value; // If the array profile is too stochastic then we'll miss the fact that the first iteration has a different indexing type than the subsequent iterations.
    for (var i = 0; i < o.length; ++i) {
        for (var j = 0; j < o.length; ++j)
            o[i] += o[j];
    }
    return o;
}

function sum(array) {
    var result = 0;
    for (var i = array.length; i--;)
        result += array[i];
    return result;
}

var result = 0;
for (var i = 0; i < 10000; ++i)
    result += sum(foo(5, i % 42));

if (result != 136889232)
    throw "Error: bad result: " + result;

