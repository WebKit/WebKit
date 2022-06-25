function test()
{
    var set = new WeakSet();
    for (var i = 0; i < 1e6; ++i) {
        set.add({});
    }
    return set;
}
noInline(test);
var set = test();
fullGC();
