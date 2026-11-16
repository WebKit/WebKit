function test(set) {
    return new Set(set);
}
noInline(test);

var set = new Set();
for (var j = 0; j < 100; ++j)
    set.add(j);

for (var i = 0; i < testLoopCount; ++i)
    test(set);
