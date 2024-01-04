function test()
{
    return new this[Symbol.species]()
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    test.call(Array);
}
