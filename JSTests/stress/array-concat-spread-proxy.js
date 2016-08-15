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
    let array = [1,2,3];
    let {proxy:p, revoke} = Proxy.revocable(array, { get : function(o, k) { return o[k]; } });

    // Test it works with proxies by default
    for (let i = 0; i < 100000; i++) {
        if (!arrayEq(Array.prototype.concat.call(p,p), [1,2,3,1,2,3]))
            throw "failed normally with a proxy"
    }

    // Test it works with spreadable false.
    p[Symbol.isConcatSpreadable] = false;
    for (let i = 0; i < 100000; i++) {
        if (!arrayEq(Array.prototype.concat.call(p,p), [p,p]))
            throw "failed with no spread"
    }

    p[Symbol.isConcatSpreadable] = undefined;
    revoke();
    passed = true;
    try {
        Array.prototype.concat.call(p,[]);
        passed = false;
    } catch (e) { }

}
