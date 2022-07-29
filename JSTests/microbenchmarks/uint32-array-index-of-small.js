let size = 16;
let array = new Uint32Array(size);
for (let i = 0; i < size; ++i)
    array[i] = 1;
array[size - 1] = 42;

function test(array) {
    let result = 0;
    for (let i = 0; i < 1e2; ++i)
        result += array.indexOf(42);
    return result;
}
noInline(test);
test(array);
