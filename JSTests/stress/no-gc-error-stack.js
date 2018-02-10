//@ runDefault

"use strict";

var errors = [];

for (let i = 0; i < 1000; ++i)
    eval("(function foo" + i + "() { errors.push(new Error()); })();");

for (let error of errors)
    error.stack;

gc();

for (let error of errors) {
    if (!error.stack.startsWith("foo")) {
        print("ERROR: Stack does not begin with foo: " + error.stack);
        throw new Error("fail");
    }
}


