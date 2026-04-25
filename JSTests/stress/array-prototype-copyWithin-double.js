function compareArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected length ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`${i}: Expected ${b[i]} but got ${a[i]}`);
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
