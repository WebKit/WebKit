//@ skip if $memoryLimited
let size = 4096;
let array = new BigInt64Array(size);
for (let i = 0; i < size; ++i)
    array[i] = 1n;
array[size - 1] = 42n;

function test(array) {
    let result = 0;
    for (let i = 0; i < 1e2; ++i)
        result += array.indexOf(42n);
    return result;
}
noInline(test);
test(array);
