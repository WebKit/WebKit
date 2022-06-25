"use strict";

function foo(x) {
    var tmp = x.f + 1;
    return tmp + arguments[0].f;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:i});
    if (result != i + i + 1)
        throw "Error: bad result: " + result;
}

var result = foo({f:4.5});
if (result != 4.5 + 4.5 + 1)
    throw "Error: bad result at end: " + result;
