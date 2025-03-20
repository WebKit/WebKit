function compareArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected length ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`${i}: Expected ${b[i]} but got ${a[i]}`);
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
