function test(b) {
    let arr = new Array(2);
    arr[0] = b;
    let obj = {};
    if (b % 2)
        obj.arr = arr;
    return arr.length + arr[0];
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
    test(i);
