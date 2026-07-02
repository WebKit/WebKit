function test(map) {
    return new Map(map);
}
noInline(test);

var map = new Map();
for (var j = 0; j < 1000; ++j)
    map.set(j, j);

for (var i = 0; i < testLoopCount; ++i)
    test(map);
