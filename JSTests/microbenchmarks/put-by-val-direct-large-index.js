//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ memoryHog!
var acc = [];
for (var i = 0; i < 1e6; i++) {
    acc.push({[5e4 + (i % 1e4)]: true});
}
