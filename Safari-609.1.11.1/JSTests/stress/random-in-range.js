function test() {
    var value = Math.random();
    if (value >= 1.0)
        return false;
    if (value < 0)
        return false;
    return true;
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    if (!test())
        throw new Error("OUT");
}
