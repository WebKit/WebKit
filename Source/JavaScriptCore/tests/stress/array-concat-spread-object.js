// This file tests is concat spreadable.

function arrayEq(a, b) {
    if (a.length !== b.length)
        return false;
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            return false;
    }
    return true;
}


{
    // let o = {0:1, 1:2, 2:3, length:3};

    // Test it works with proxies by default
    for (let i = 0; i < 100000; i++) {
        if (!arrayEq(Array.prototype.concat.call(o,o), [o,o]))
            throw "failed normally with an object"
    }

    // Test it works with spreadable true
    o[Symbol.isConcatSpreadable] = true;
    for (let i = 0; i < 100000; i++) {
        let result = Array.prototype.concat.call(o,o)
        if (!arrayEq(result, [1,2,3,1,2,3]))
            throw "failed with spread got: " + result;
    }

    // Test it works with many things
    o[Symbol.isConcatSpreadable] = true;
    let other = {}
    for (let i = 0; i < 100000; i++) {
        let result = Array.prototype.concat.call(o,o,true,[1,2],other)
        if (!arrayEq(result, [1,2,3,1,2,3,true,1,2,other]))
            throw "failed with spread got: " + result;
    }

    // Test it works with strings
    String.prototype[Symbol.isConcatSpreadable] = true;
    for (let i = 0; i < 100000; i++) {
        let result = Array.prototype.concat.call("hi","hi")
        // This is what the spec says is the correct answer... D:
        if (!arrayEq(result, ["h", "i", "hi"]))
            throw "failed with string got: " + result + " on iteration " + i;
    }
}
