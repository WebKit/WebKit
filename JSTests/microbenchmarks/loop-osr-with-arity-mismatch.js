//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(x, y) {
    for (var i = 0; i < 1e7; i++) {
        x[y] += i;
    }
}
foo({})
