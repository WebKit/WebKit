//@ runDefault("--useConcurrentJIT=0")
function test()
{
    for (var i = 0; i < 100; ++i) {
        var a = WeakSet.bind();
        var b = new Proxy(a, a);
        var c = new Promise(b);
        var d = WeakSet.bind();
        var e = new Proxy(d, WeakSet);
        var f = new Promise(e);
    }
}

for (var i = 0; i < 50; ++i)
    test();
