function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function doSplit(string, separator, limit) {
    return string.split(separator, limit);
}
noInline(doSplit);

// Train with an object limit so the DFG does not speculate the limit argument
// as Int32 and OSR-exit before reaching operationStringSplitRegExp.
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(JSON.stringify(doSplit("a,b,c", /,/, { valueOf() { return 4; } })), '["a","b","c"]');

// A separator with an own undefined @@split keeps the primordial watchpoints
// intact but routes operationStringSplitRegExp to its no-@@split fallback,
// which must evaluate ToUint32(limit) before ToString(separator).
let order = [];
let re = /,/;
re[Symbol.split] = undefined;
re.toString = function() {
    order.push("separator");
    return ",";
};
let limit = {
    valueOf() {
        order.push("limit");
        return 4;
    }
};
let result = doSplit("a,b,c", re, limit);
shouldBe(JSON.stringify(order), '["limit","separator"]');
shouldBe(JSON.stringify(result), '["a","b","c"]');
