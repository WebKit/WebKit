//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(arr, index) {
    for (let i = 0; i < 1e2; i++) {
        let x = {};
        x.x = arr;
    }

    return arr[index];
}
noInline(foo);

const arr = new Array(10).fill({});
for (let i = 0; i < 1e6; i++) {
    foo(arr, i % arr.length);
}
for (let i = 0; i < 1e6; i++) {
    foo(arr, i % arr.length);
    if (!(i % arr.length))
        foo(arr, -1);
}
