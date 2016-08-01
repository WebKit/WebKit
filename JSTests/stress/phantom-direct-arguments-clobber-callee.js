function foo() {
    return function() { return effectful42.apply(this, arguments) };
}
noInline(foo);

function bar(a) {
    var result = foo()();
    return [result, result, result, result, result, result, result, result, result + a];
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = "" + bar(1);
    if (result != "42,42,42,42,42,42,42,42,43")
        throw "Error: bad result: " + result;
}

var result = "" + bar(2147483647);
if (result != "42,42,42,42,42,42,42,42,2147483689")
    throw "Error: bad result at end: " + result;
