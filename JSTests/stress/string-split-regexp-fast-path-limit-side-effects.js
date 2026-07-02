function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function doSplit(string, separator, limit) {
    return string.split(separator, limit);
}
noInline(doSplit);

for (let i = 0; i < testLoopCount; ++i)
    shouldBe(JSON.stringify(doSplit("a,b,c", /,/, 4)), '["a","b","c"]');

let execCalled = false;
let evil = {
    valueOf() {
        RegExp.prototype.exec = function() {
            execCalled = true;
            return null;
        };
        return 4;
    }
};
let result = doSplit("a,b,c", /,/, evil);
shouldBe(execCalled, true);
shouldBe(JSON.stringify(result), '["a,b,c"]');
