function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


var set = new Set;
for (var i = 0; i < 100; ++i)
    set.add(i);

function test(set) {
    var result = 0;
    for (let a of set) {
        result += a;
    }
    return result;
}
noInline(test);

const iterationCount = $vm.useJIT() ? 1e6 : 1e4;
for (var i = 0; i < iterationCount; ++i)
    shouldBe(test(set), 4950);
