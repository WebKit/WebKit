function foo(a) {
    return typeof(a) == "undefined";
}

var array = ["string", 5, 6.5, void(0), null, true, false, {f:42}, [1, 2, 3]];

var result = 0;
for (var i = 0; i < 100000; ++i) {
    result *= 3;
    result += foo(array[i % array.length]) | 0;
    result |= 0;
}

if (result != -1076651747)
    throw "Bad result: " + result;
