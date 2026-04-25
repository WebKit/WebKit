function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`${i}: Expected ${b[i]} but got ${a[i]}`);
    }
}

{
    // ArrayWithInt32 * Int32
    const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    arr.fill(12, 2, 7);
    sameArray(arr, [1, 2, 12, 12, 12, 12, 12, 8, 9]);
}

{
    // ArrayWithInt32 * Double
    const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    arr.fill(1.2, 2, 7);
    sameArray(arr, [1, 2, 1.2, 1.2, 1.2, 1.2, 1.2, 8, 9]);
}

{
    // ArrayWithInt32 * Other
    const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    const obj = {};
    arr.fill(obj, 2, 7);
    sameArray(arr, [1, 2, obj, obj, obj, obj, obj, 8, 9]);
}

{
    // ArrayWithDouble * Int32
    const arr = [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9];
    arr.fill(12, 2, 7);
    sameArray(arr, [1.1, 1.2, 12, 12, 12, 12, 12, 1.8, 1.9]);
}

{
    // ArrayWithDouble * Double
    const arr = [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9];
    arr.fill(12.1, 2, 7);
    sameArray(arr, [1.1, 1.2, 12.1, 12.1, 12.1, 12.1, 12.1, 1.8, 1.9]);

}

{
    // ArrayWithDouble * Other
    const arr = [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9];
    const obj = {};
    arr.fill(obj, 2, 7);
    sameArray(arr, [1.1, 1.2, obj, obj, obj, obj, obj, 1.8, 1.9]);
}

{
    // ArrayWithContiguous * Int32
    const obj = {};
    const arr = [obj, obj, obj, obj, obj, obj, obj, obj, obj];
    arr.fill(12, 2, 7);
    sameArray(arr, [obj, obj, 12, 12, 12, 12, 12, obj, obj]);
}

{
    // ArrayWithContiguous * Double
    const obj = {};
    const arr = [obj, obj, obj, obj, obj, obj, obj, obj, obj];
    arr.fill(12.1, 2, 7);
    sameArray(arr, [obj, obj, 12.1, 12.1, 12.1, 12.1, 12.1, obj, obj]);
}

{
    // ArrayWithContiguous * Double
    const obj = {};
    const arr = [obj, obj, obj, obj, obj, obj, obj, obj, obj];
    const obj2 = {};
    arr.fill(obj2, 2, 7);
    sameArray(arr, [obj, obj, obj2, obj2, obj2, obj2, obj2, obj, obj]);
}

{
    // ArrayWithUndecided * Int32
    const arr = new Array(9);
    arr.fill(12, 2, 7);
    sameArray(arr, [undefined, undefined, 12, 12, 12, 12, 12, undefined, undefined]);
}

{
    // ArrayWithUndecided * Double
    const arr = new Array(9);
    arr.fill(12.1, 2, 7);
    sameArray(arr, [undefined, undefined, 12.1, 12.1, 12.1, 12.1, 12.1, undefined, undefined]);
}

{
    // ArrayWithUndecided * Double
    const arr = new Array(9);
    const obj = {};
    arr.fill(obj, 2, 7);
    sameArray(arr, [undefined, undefined, obj, obj, obj, obj, obj, undefined, undefined]);
}
