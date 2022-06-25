function test() {
    var MAX = 50;
    var found53Bit = false;
    var foundLessThan53Bit = false;
    var results = new Array(MAX);

    for (var i = 0; i < MAX; ++i) {
        var str = Math.random().toString(2);
        results[i] = str;
        // 53 bit + '0.'.length
        if (str.length === (53 + 2))
            found53Bit = true;
        else if (str.length < (53 + 2))
            foundLessThan53Bit = true;

        if (found53Bit && foundLessThan53Bit)
            return true;
    }
    print(`Random seed ${getRandomSeed()}`);
    print(results.join('\n'));
    return false;
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    if (!test())
        throw new Error("OUT");
}
