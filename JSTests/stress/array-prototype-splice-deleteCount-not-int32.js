function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected length is ${b.length} but got length ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`Expected ${b[i]} but got ${a[i]} (${i})`);
    }
}

{
    const array = [1, 2, 3, 4, 5];
    array.splice(1, true);
    sameArray(array, [1, 3, 4, 5]);
}

{
    const array = [1, 2, 3, 4, 5];
    array.splice(1, false);
    sameArray(array, [1, 2, 3, 4, 5]);
}

{
    const array = [1, 2, 3, 4, 5];
    array.splice(1, null);
    sameArray(array, [1, 2, 3, 4, 5]);
}

{
    const array = [1, 2, 3, 4, 5];
    array.splice(1, undefined);
    sameArray(array, [1, 2, 3, 4, 5]);
}

{
    const array = [1, 2, 3, 4, 5];
    array.splice(1, NaN);
    sameArray(array, [1, 2, 3, 4, 5]);
}

{
    const array = [1, 2, 3, 4, 5];
    array.splice(1, 1.24);
    sameArray(array, [1, 3, 4, 5]);
}
