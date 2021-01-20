//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
const array = [];
for (let i = 0; i < 100; ++i)
    array.push(new Array(1e6).join('—' + i));
