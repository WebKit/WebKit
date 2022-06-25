function* gen()
{
    var ok = 2;
    ok = ok + (yield 42);
    yield ok;
    yield ok;
    yield ok;
}

var g = gen();
g.next();
$vm.enableDebuggerModeWhenIdle();
fullGC();
g.next();
