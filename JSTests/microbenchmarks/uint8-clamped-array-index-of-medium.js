let size = 4096;
let array = new Uint8ClampedArray(size);
for (let i = 0; i < size; ++i)
    array[i] = 1;
array[size - 1] = 42;

function test(array) {
    let result = 0;
    for (let i = 0; i < 1e4; ++i)
        result += array.indexOf(42);
    return result;
}
noInline(test);
test(array);
