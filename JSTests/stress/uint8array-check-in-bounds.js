function test(array)
{
    let result = 0;
    for (let i = 0; i < array.length; ++i)
        result += array[i];
    return array;
}
noInline(test);

let array = new Uint8Array(1024);
for (let i = 0; i < testLoopCount; ++i)
    test(array);
