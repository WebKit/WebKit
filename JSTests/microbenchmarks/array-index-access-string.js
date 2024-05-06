function test(array, index) {
    return array[index];
}
noInline(test);

var array = [0, 1, 2, 3, 4, 5, 6, 7];

for (var i = 0; i < 1e6; ++i) {
    test(array, i & 7);
    test(array, "0");
}
