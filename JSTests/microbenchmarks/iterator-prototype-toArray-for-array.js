//@ requireOptions("--useIteratorHelpers=1")

var a1 = new Array(1024);
a1.fill(99);
for (var i = 0; i < 1e2; ++i)
    var a2 = Iterator.prototype.toArray.call(a1);
