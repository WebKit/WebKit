function test(op1, op2) {
    return op1 * op2;
}
noInline(test);

var array = [
    "124",
    "15",
];
for (var i = 0; i < 1e6; ++i) {
    test(array[i & 0x1], i);
}
