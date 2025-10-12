function test(b) {
    let arr = new Array(8);
    arr[1] = b;
    arr[3] = arr.length; 
    arr[5] = arr.length; 
    return arr[1] + arr[3] + arr[5];
}
noInline(test);

for (let i = 0; i < testLoopCount; i++)
    test(i);
