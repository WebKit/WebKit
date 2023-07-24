var array = [];
for (var i = 0; i < 1024; ++i)
    array.push(i + 0.5);

function test(array) {
    return new Float64Array(array);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test(array);
