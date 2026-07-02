function test() {
    var arr = [3, 4, /Ῠ/iu];
    return arr.sort(function() { arr.sort(function() {}); });
}

for (var i = 0; i < testLoopCount; ++i)
    test();
