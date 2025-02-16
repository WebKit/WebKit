function test()
{
    return new this[Symbol.species]()
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i) {
    test.call(Array);
}
