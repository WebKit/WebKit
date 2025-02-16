function test()
{
    var map = new WeakMap();
    for (var i = 0; i < testLoopCount; ++i) {
        map.set({}, i);
    }
    return map;
}
noInline(test);
var map = test();
fullGC();
