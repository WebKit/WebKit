// Regression test: the optimized RegExp.prototype[Symbol.split] path must honor an
// overwritten RegExp[Symbol.species]. A plain /,/ resolves its species through RegExp, so
// the split must use the ";"-splitter returned here instead of the original "," pattern.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

const speciesDescriptor = Object.getOwnPropertyDescriptor(RegExp, Symbol.species);
Object.defineProperty(RegExp, Symbol.species, {
    configurable: true,
    get() {
        return function (re, flags) {
            return new RegExp(";", flags);
        };
    }
});

const rx = /,/;

function doSplit(s) {
    return rx[Symbol.split](s);
}
noInline(doSplit);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(JSON.stringify(doSplit("a,b;c,d")), '["a,b","c,d"]');

Object.defineProperty(RegExp, Symbol.species, speciesDescriptor);
