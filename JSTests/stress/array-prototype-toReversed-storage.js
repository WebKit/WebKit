function sameArray(a, b) {
    if (a.length !== b.length)
        throw new Error(`Expected array length ${b.length} but got ${a.length}`);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(`Expected ${b[i]} but got ${a[i]} (${i})`);
    }
}


{
    const arr = [];
    for (let i = 0; i < 5; i++) {
        arr.push(i);
    }
    arr.length = 1000000000;
    arr.length = 5;

    const result = arr.toReversed();

    sameArray(result, [4, 3, 2, 1, 0]);
}

{
    const arr = [];
    for (let i = 0; i < 5; i++) {
        arr.push(i + 0.5);
    }
    arr.length = 1000000000;
    arr.length = 5;

    const result = arr.toReversed();

    sameArray(result, [4.5, 3.5, 2.5, 1.5, 0.5]);
}

{
    const arr = [];
    for (let i = 0; i < 5; i++) {
        arr.push({ i });
    }
    arr.length = 1000000000;
    arr.length = 5;

    const result = arr.toReversed();

    sameArray(result, [arr[4], arr[3], arr[2], arr[1], arr[0]]);
}
