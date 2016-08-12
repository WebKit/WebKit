function foo() {
    return effectful42.apply(this, arguments);
}

function bar(a, b) {
    var result = foo.apply(this, b);
    return [a, a, a, a, a, a, a, a, result + a];
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = "" + bar(1, []);
    if (result != "1,1,1,1,1,1,1,1,43")
        throw "Error: bad result: " + result;
}

var result = "" + bar(2147483647, []);
if (result != "2147483647,2147483647,2147483647,2147483647,2147483647,2147483647,2147483647,2147483647,2147483689")
    throw "Error: bad result at end: " + result;
