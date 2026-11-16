function compareArray(actual, expected) {
    if (actual.length !== expected.length)
        throw new Error(`Expected length ${expected.length} but got ${actual.length}`);
    for (let i = 0, l = actual.length; i < l; ++i) {
        if (Object.hasOwn(actual, i) !== Object.hasOwn(expected, i))
            throw new Error(`${i}: Mismatch the owner of the property`);
        if (actual[i] !== expected[i])
            throw new Error(`${i}: Expected ${expected[i]} but got ${actual[i]}`);
    }
}

{
    let arr = [0, 1, 2, 3, 4, 5, 6, 7];
    arr.copyWithin(0, 3);
    compareArray(arr, [3, 4, 5, 6, 7, 5, 6, 7]);
}

{
    let arr = [100, 200, 300, 400];
    arr.copyWithin(2, 0, 2);
    compareArray(arr, [100, 200, 100, 200]);
}

{
    let arr = [10, 11, 12, 13, 14];
    arr.copyWithin(-2, -3);
    compareArray(arr, [10, 11, 12, 12, 13]);
}

{
    let arr = [0, 1, 2, 3, 4, 5];
    arr.copyWithin(2, 3);
    compareArray(arr, [0, 1, 3, 4, 5, 5]);
}

{
    // Have a hole on array by literal.
    const arr = [0, , 2, , 4, , 6];
    arr.copyWithin(2, 3);
    compareArray(arr, [0, , , 4, , 6, 6]);
}

{
    // Have a hole by array constructor.
    const arr = new Array(7);
    arr[0] = 0;
    arr[2] = 2;
    arr[4] = 4;
    arr[6] = 6;
    arr.copyWithin(2, 3);
    compareArray(arr, [0, , , 4, , 6, 6]);
}

{
    // Have a hole by chainging Array#length
    const arr = new Array(0);
    arr.length = 5;
    arr[2] = 2;
    arr.copyWithin(2, 0);
    compareArray(arr, [, , , , 2]);
}

{
    // Have a hole by delete indexed prop
    const arr = [0, 1, 2];
    delete arr[1];
    arr.copyWithin(1, 1);
    compareArray(arr, [0, , 2]);
}