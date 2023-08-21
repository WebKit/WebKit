let array = new Uint8ClampedArray(1);
function foo(value) {
    array[0] = value;
}
noInline(foo);

for (let i = 0; i < 1e6; ++i) {
    foo(2.5);
    if (array[0] != 2)
        throw new Error(i);

    foo(3.5);
    if (array[0] != 4)
        throw new Error(i);
}