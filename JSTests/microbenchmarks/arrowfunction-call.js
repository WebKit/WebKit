//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var af = (a, b) => a + b;

noInline(af);

function bar(a, b) {
    return af(a, b);
}

noInline(bar);

for (let i = 0; i < 1000000; ++i) {
    let result = bar(1, 2);
    if (result != 3)
        throw "Error: bad result: " + result;
}
