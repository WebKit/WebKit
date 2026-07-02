function test(array, key, value) {
    array.with(key, value);
}
noInline(test);

var array = [
    { value: 12 }, { value: 4 }, { value: 4 }, { value: 66 }, { value: 56 },
    { value: 44 }, { value: 44 }, { value: 38 }, { value: 65 }, { value: 89 },
    { value: 2 }, { value: 45 }, { value: 789 }, { value: 56 }, { value: 432 },
    { value: 677 }, { value: 2234 }, { value: 77 }, { value: 3457 }, { value: 133 },
    { value: 6544 }, { value: 76 }, { value: 17 }, { value: 322 }, { value: 34 }
];

for (let i = 0; i < 1e6; ++i) {
    test(array, 14, { value: 34 });
}
