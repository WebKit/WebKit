// This tests that integer divisions are appropriately optimized, while double
// divisions are still kept the same as before.

function foo(a, b) {
    return a / b;
}

function bar(a, b) {
    return (a / b) | 0;
}

var result = 0;

for (var i = 0; i < 1000000; ++i) {
    var a;
    var b;
    if (i < 500) {
        a = i * 2;
        b = 2;
    } else {
        a = i * 3;
        b = 4;
    }
    
    result += foo(a, b) * 3 + bar(a, b);
}

if (result != 1499998249937.5) {
    print("Bad result: " + result);
    throw "Error";
}

