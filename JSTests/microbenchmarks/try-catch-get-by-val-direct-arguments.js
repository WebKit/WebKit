function foo() {
    try {
        return arguments[0];
    } catch (e) {
        return 42;
    }
}

var n = 100000;
var result = 0;
for (var i = 0; i < n; ++i)
    result += foo(24);

if (result != n * 24)
    throw "Error: bad result: " + result;

result = foo();
if (result !== void 0)
    throw "Error: bad result at end: " + result;
