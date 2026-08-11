function test()
{
    var set = new WeakSet();
    for (var i = 0; i < testLoopCount; ++i) {
        set.add({});
    }
    return set;
}
noInline(test);
var set = test();
fullGC();
