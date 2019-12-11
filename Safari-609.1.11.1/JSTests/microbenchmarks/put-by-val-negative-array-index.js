//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(arr, index) {
    arr[index] = index;

    for (let j = 0; j < 1e2; j++) {
        let x = {};
        x.x = arr;
    }
}
noInline(foo);

const arr = new Array(10).fill({});
let result = 0;
for (let i = 0; i < 1e6; i++) {
    result += foo(arr, i % arr.length);
}
for (let i = 0; i < 1e6; i++) {
    result += foo(arr, i % arr.length);
    if (!(i % arr.length))
        result += foo(arr, -1);
}
