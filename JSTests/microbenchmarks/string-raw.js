//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(x, y) {
    return String.raw`a\t${x}b\n${y}c`;
}
noInline(test);

var total = 0;
for (var i = 0; i < 1e6; ++i)
    total += test(i % 10, (i + 1) % 10).length;

if (total !== 9000000)
    throw "Error: bad result: " + total;
