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
    let arr = [1.5, 2, 3.25, 4, 5.75, 6, 7.125, 8];
    arr.copyWithin(3, 0, 2); 
    compareArray(arr, [1.5, 2, 3.25, 1.5, 2, 6, 7.125, 8]);
}

{
    let arr = [100.5, 200.1, 300, 400.9, 500];
    arr.copyWithin(3, 1, 3); 
    compareArray(arr, [100.5, 200.1, 300, 200.1, 300]);
}

{
    let arr = [1, 2, 3.3, 4.4];
    arr.copyWithin(0, 3, 2);
    compareArray(arr, [1, 2, 3.3, 4.4]);
}

{
    // Have a hole on array by literal.
    const arr = [0.0, , 2.2, , 4.4, , 6.6];
    arr.copyWithin(2, 3);
    compareArray(arr, [0.0, , , 4.4, , 6.6, 6.6]);
}

{
    // Have a hole by array constructor.
    const arr = new Array(7);
    arr[0] = 0.0;
    arr[2] = 2.2;
    arr[4] = 4.4;
    arr[6] = 6.6;
    arr.copyWithin(2, 3);
    compareArray(arr, [0.0, , , 4.4, , 6.6, 6.6]);
}

{
    // Have a hole by chainging Array#length
    const arr = new Array(0);
    arr.length = 5;
    arr[2] = 2.2;
    arr.copyWithin(2, 0);
    compareArray(arr, [, , , , 2.2]);
}

{
    // Have a hole by delete indexed prop
    const arr = [0.0, 1.1, 2.2];
    delete arr[1];
    arr.copyWithin(1, 1);
    compareArray(arr, [0, , 2.2]);
}