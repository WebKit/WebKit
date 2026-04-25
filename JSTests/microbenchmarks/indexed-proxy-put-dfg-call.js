var array = new Proxy([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19], Reflect);

function test() {
    for (var i = 0; i < array.length; ++i)
        array[i] = i;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
