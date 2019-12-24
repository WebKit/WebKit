"use strict";

function foo(a, b, ...rest) {
    return rest.length;
}

function bar(a, b, ...rest) {
    return foo.call(...rest);
}
noInline(bar);

let array = new Array(10);
for (let i = 0; i < 1e5; ++i) {
    let result = bar(...array);
    if (result !== array.length - bar.length - foo.length - 1)
        throw new Error(i + " " + result);
}

array.length = 10000;
if (bar(...array) !== array.length - bar.length - foo.length - 1)
    throw new Error();
