function assert(b, i) {
    if (!b)
        throw new Error("Bad! " + i)
}

let f = function() {
    return this;
}
noInline(f);

let fStrict = function() {
    "use strict";
    return this;
}
noInline(fStrict);

const globalThis = this;
for (let i = 0; i < 1000; i++)
    assert(f() === globalThis, i);

for (let i = 0; i < 1000; i++)
    assert(fStrict() === undefined);
