"use strict";

var errors = [];

for (let i = 0; i < 1000; ++i)
    eval("(function foo" + i + "() { errors.push(new Error()); })();");

gc();

let didGCAtLeastOne = false;
for (let error of errors) {
    if (!error.stack.startsWith("foo"))
        didGCAtLeastOne = true;
}

if (!didGCAtLeastOne)
    throw new Error("Bad result: didGCAtLeastOne = " + didGCAtLeastOne);

