let arrays = [
    [0,0],
    [0,1],
    [1,0],
    [1,1],
];

function foo(arr) {
    return arr.toString();
}
noInline(foo);

for (let i = 0; i < 30000; ++i) {
    for (let arr of arrays)
        foo(arr);
}
