function foo(x) {
    return "" + x;
}

var result;
var limit = 100000;
for (var i = 0; i < limit; ++i)
    result = foo(i);

if (result != String(limit - 1))
    throw "Error: bad result: " + result;
