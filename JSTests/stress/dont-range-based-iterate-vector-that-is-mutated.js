// Should not crash when run with ASAN.

function foo(arr1, arr2) {
    if (arr1.length != arr2.length) {
        return;
    }
    for (let i = 0; i < arr1.length; i++) {
        arr1[i] = arr2[i];
    }
}

let arr1 = new Int32Array(100);

for (let i = 0; i < 10000; i++) {
    foo(arr1, arr1);
}
