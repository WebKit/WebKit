"use strict";

function foo() {
    function bar(i) {
        return this;
    }
    function inner() {
        let result;
        for (let i = 0; i < testLoopCount; i++)
            result = bar(i);
        return result;
    }
    noInline(inner);
    return inner();
}

let result = foo();
if (result !== undefined)
    throw new Error("Bad result");
