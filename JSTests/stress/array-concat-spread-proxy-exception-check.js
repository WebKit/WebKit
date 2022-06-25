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
    let concat = Array.prototype.concat;
    noInline(concat);
    let array = [1, 2, 3];
    let {proxy:p, revoke} = Proxy.revocable(array, { get : function(o, k) { return o[k]; } });

    concat.call(p,p);

    for (let i = 0; i < 100000; i++) {
        if (!arrayEq(concat.call(p,p), [1,2,3,1,2,3]))
            throw "bad";
    }
    revoke();
    failed = true;
    try {
        concat.call(p,p);
    } catch (e) {
        failed = false;
    }

    if (failed)
        throw "bad"

}
