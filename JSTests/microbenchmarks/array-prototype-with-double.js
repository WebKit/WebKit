function test(array, key, value) {
    array.with(key, value);
}
noInline(test);

var array = [12.5, 4.5, 4.5, 66.5, 56.5, 44.5, 44.5, 38.5, 65.5, 89.5, 2.5, 45.5, 789.5, 56.5, 432.5, 677.5, 2234.5, 77.5, 3457.5, 133.5, 6544.5, 76.5, 17.5, 322.5, 34.5];

for (let i = 0; i < 1e6; ++i) {
    test(array, 14, 324.6);
}
