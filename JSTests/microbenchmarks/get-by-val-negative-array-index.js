function foo(arr, index) {
    return arr[index];
}
noInline(foo);

const arr = new Array(1000).fill({});
for (let i = 0; i < 1e7; i++) {
    foo(arr, i % arr.length);
    if (!(i % 1e3))
        foo(arr, -1);
}
