var globalResult;
Object.prototype.valueOf = function() { globalResult = 1; }

function foo(p) {
    globalResult = 0;
    var o = arguments;
    if (p)
        +o;
    return globalResult;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(false);
    if (result !== 0)
        throw "Error: bad result: " + result;
}

var result = foo(true);
if (result !== 1)
    throw "Error: bad result at end: " + result;

