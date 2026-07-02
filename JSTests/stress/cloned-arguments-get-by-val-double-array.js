function foo() {
    "use strict";
    return arguments[0] + 1.5;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo(4.2);
    if (result != 5.7)
        throw "Error: bad result: " + result;
}

