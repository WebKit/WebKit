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

    array = [1,2];
    Object.defineProperty(array, 2, { get: () => { return 1; } });

    for (let i = 0; i < 100000; i++) {
        if (!arrayEq(Array.prototype.concat.call(array,array), [1,2,1,1,2,1]))
            throw "failed normally with a getter"
        if (!arrayEq(Array.prototype.concat.call([],array), [1,2,1]))
            throw "failed with undecided and a getter"
    }

    // Test with indexed types on prototype.
    array = [1,2];
    array.length = 3;
    Array.prototype[2] = 1;

    for (let i = 0; i < 100000; i++) {
        if (!arrayEq(Array.prototype.concat.call(array,array), [1,2,1,1,2,1]))
            throw "failed normally with an indexed prototype"
        if (!arrayEq(Array.prototype.concat.call([],array), [1,2,1]))
            throw "failed with undecided and an indexed prototype"
    }
}
