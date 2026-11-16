for (let i = 0; i < 20; i++) {
    let array = new Array(i);
    for (let j = 0; j < i; j++)
        array[j] = 1;

    for (let j = 0; j < 1e4; ++j)
        array.map((x) => x);
}
