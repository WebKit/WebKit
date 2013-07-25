function foo(a) {
    var result = 0;
    for (var i = 0; i < a.length; ++i) {
        result <<= 1;
        result ^= "foo" in a[i];
    }
    return result;
}

function bar() {
    var array = [{bar:42}, {bar:42}];
    var result = 0;
    for (var i = 0; i < 1000000; ++i)
        result += foo(array);
    return result;
}

var result = bar();
if (result != 0)
    throw "Bad result: " + result;

