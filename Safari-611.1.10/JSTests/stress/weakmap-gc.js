function test()
{
    var map = new WeakMap();
    for (var i = 0; i < 1e6; ++i) {
        map.set({}, i);
    }
    return map;
}
noInline(test);
var map = test();
fullGC();
