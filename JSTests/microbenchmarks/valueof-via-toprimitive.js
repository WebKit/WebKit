var array = [];
function test(data) {
    return Number(data);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(array);
