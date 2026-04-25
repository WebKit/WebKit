function test(b) {
    let arr = new Array(4);
    if (b % 3 === 0)
        arr[0] = b;
    if (b % 5 === 0)
        return arr.length;
    if (b % 7 === 0)
        return arr[1];
    return arr.length + arr[2];
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
    test(i);
