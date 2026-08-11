
function leak(x) {
    return x;
}

function test(b) {
    let arr = new Array(4);
    arr[1] = 10;
    if (b % 4 === 0)
        leak(arr);
    return arr.length + arr[1];
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
    test(i);
