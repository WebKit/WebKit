function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected length ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`${i}: Expected ${b[i]} but got ${a[i]}`);
    }
}

{
    const array = [1, 2, 3, 4, 5, 6, 7];
    ensureArrayStorage(array);
    sameArray(array.copyWithin(0, 1, 4), [2, 3, 4, 4, 5, 6, 7]);
    sameArray(array.copyWithin(2, 3, 6), [2, 3, 4, 5, 6, 6, 7]);
}

{
    const array = ["x", "y", "z", "a", "b", "c"];
    ensureArrayStorage(array);
    sameArray(array.copyWithin(0, -2, -1), ["b", "y", "z", "a", "b", "c"]);
    sameArray(array.copyWithin(3, 1), ["b", "y", "z", "y", "z", "a"]);
}

{
    const array = [10, 20, 30, 40, 50, 60];
    ensureArrayStorage(array);
    sameArray(array.copyWithin(4, 0, 2), [10, 20, 30, 40, 10, 20]);
    sameArray(array.copyWithin(2, 1, 5), [10, 20, 20, 30, 40, 10]);
}

{
    const array = [1, 2, 3, 4, 5];
    ensureArrayStorage(array);
    sameArray(array.copyWithin(1), [1, 1, 2, 3, 4]);
    sameArray(array.copyWithin(), [1, 1, 2, 3, 4]);
}

{
    const array = ["a", "b", "c", "d", "e", "f", "g", "h"];
    ensureArrayStorage(array);
    sameArray(array.copyWithin(2, 0, 6), ["a", "b", "a", "b", "c", "d", "e", "f"]);
}

{
    const array = new Array(10).fill(0).map((_, i) => i + 1);
    ensureArrayStorage(array);
    sameArray(array.copyWithin(0, 3, 8), [4, 5, 6, 7, 8, 6, 7, 8, 9, 10]);
    sameArray(array.copyWithin(1, 5, 9), [4, 6, 7, 8, 9, 6, 7, 8, 9, 10]);
}

