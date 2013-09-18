"use strict";

function foo(args, i) {
    return args[i];
}

function bar(i) {
    return foo(arguments, i);
}

noInline(foo);
noInline(bar);

while (!dfgCompiled({f:foo}))
    bar(0);

for (var i = 1; i <= 1000; i++) {
    var result = bar(i);
    if (result !== undefined)
        throw "Expected undefined at index " + i + ", got: " + result;
}
