var globalResult;
Object.prototype.valueOf = function() { globalResult = 1; }

function foo() {
    globalResult = 0;
    +arguments;
    return globalResult;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo();
    if (result !== 1)
        throw "Error: bad result: " + result;
}

