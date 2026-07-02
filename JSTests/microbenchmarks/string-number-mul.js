function test(op1, op2) {
    return op1 * op2;
}
noInline(test);

var array = [
    "0.124",
    "15.344",
];
for (var i = 0; i < 1e6; ++i) {
    test(array[i & 0x1], i * 0.5);
}
