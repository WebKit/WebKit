"use strict";

function assert(b) {
    if (!b)
        throw new Error;
}

function assertWrapper(a, b, c) {
    return assert.apply(null, arguments);
}
noInline(assertWrapper);
noFTL(assertWrapper);

let start = Date.now();
for (let i = 0; i < 1000000; ++i) {
    assertWrapper(true, true, true);
}

if (false)
    print(Date.now() - start);
