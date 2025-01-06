function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected length ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i]) {
            throw new Error(`Index ${i}: Expected ${b[i]} but got ${a[i]}`);
        }
    }
}

// ArrayWithInt32 * Int32
{
    const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    const newArr = arr.with(2, 12);
    sameArray(arr, [1, 2, 3, 4, 5, 6, 7, 8, 9]);
    sameArray(newArr, [1, 2, 12, 4, 5, 6, 7, 8, 9]);
}

// ArrayWithInt32 * Double
{
    const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    const newArr = arr.with(2, 1.2);
    sameArray(arr, [1, 2, 3, 4, 5, 6, 7, 8, 9]);
    sameArray(newArr, [1, 2, 1.2, 4, 5, 6, 7, 8, 9]);
}

// ArrayWithInt32 * Contiguous
{
    const arr = [1, 2, 3, 4, 5, 6, 7, 8, 9];
    const obj = {};
    const newArr = arr.with(2, obj);
    sameArray(arr, [1, 2, 3, 4, 5, 6, 7, 8, 9]);
    sameArray(newArr, [1, 2, obj, 4, 5, 6, 7, 8, 9]);
}

// ArrayWithDouble * Int32
{
    const arr = [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9];
    const newArr = arr.with(2, 12);
    sameArray(arr, [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9]);
    sameArray(newArr, [1.1, 1.2, 12, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9]);
}

// ArrayWithDouble * Double
{
    const arr = [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9];
    const newArr = arr.with(2, 12.1);
    sameArray(arr, [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9]);
    sameArray(newArr, [1.1, 1.2, 12.1, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9]);
}

// ArrayWithDouble * Other
{
    const arr = [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9];
    const obj = {};
    const newArr = arr.with(2, obj);
    sameArray(arr, [1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9]);
    sameArray(newArr, [1.1, 1.2, obj, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9]);
}

// ArrayWithContiguous * Int32
{
    const shared = {};
    const arr = [shared, shared, shared, shared, shared, shared, shared, shared, shared];
    const newArr = arr.with(2, 12);
    sameArray(arr, [shared, shared, shared, shared, shared, shared, shared, shared, shared]);
    sameArray(newArr, [shared, shared, 12, shared, shared, shared, shared, shared, shared]);
}

// ArrayWithContiguous * Double
{
    const shared = {};
    const arr = [shared, shared, shared, shared, shared, shared, shared, shared, shared];
    const newArr = arr.with(2, 12.1);
    sameArray(arr, [shared, shared, shared, shared, shared, shared, shared, shared, shared]);
    sameArray(newArr, [shared, shared, 12.1, shared, shared, shared, shared, shared, shared]);
}

// ArrayWithContiguous * Other
{
    const shared = {};
    const arr = [shared, shared, shared, shared, shared, shared, shared, shared, shared];
    const obj2 = {};
    const newArr = arr.with(2, obj2);
    sameArray(arr, [shared, shared, shared, shared, shared, shared, shared, shared, shared]);
    sameArray(newArr, [shared, shared, obj2, shared, shared, shared, shared, shared, shared]);
}
