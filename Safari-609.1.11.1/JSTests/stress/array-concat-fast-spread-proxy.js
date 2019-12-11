// This file tests is concat spreadable when taking the fast path
// (single argument, JSArray receiver)

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
    let array = [1,2,3];
    let {proxy:p, revoke} = Proxy.revocable([4, 5], {});

    // Test it works with proxies by default
    for (let i = 0; i < 10000; i++) {
        if (!arrayEq(Array.prototype.concat.call(array, p), [1,2,3,4,5]))
            throw "failed normally with a proxy"
    }

    // Test it works with spreadable false.
    p[Symbol.isConcatSpreadable] = false;
    for (let i = 0; i < 10000; i++) {
        if (!arrayEq(Array.prototype.concat.call(array,p), [1,2,3,p]))
            throw "failed with no spread"
    }

    p[Symbol.isConcatSpreadable] = undefined;
    revoke();
    passed = true;
    try {
        Array.prototype.concat.call(array,p);
        passed = false;
    } catch (e) { }
    if (!passed)
        throw "failed to throw spreading revoked proxy";
}
