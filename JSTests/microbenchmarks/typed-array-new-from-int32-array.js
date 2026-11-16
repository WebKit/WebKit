var array = [];
for (var i = 0; i < 1024; ++i)
    array.push(i);

function test(array) {
    return new Uint32Array(array);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test(array);
