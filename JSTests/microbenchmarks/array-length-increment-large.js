function test(count) {
    const array = [];
    for (let i = 0; i < count; i++) {
        ++array.length;
        array[i] = i;
    }
    return array;
}
noInline(test);

for (let i = 0; i < 3; i++) {
    const array = test(150000);
    if (array.length !== 150000 || array[149999] !== 149999)
        throw new Error("bad result");
}
