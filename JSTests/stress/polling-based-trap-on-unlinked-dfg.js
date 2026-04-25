//@ runDefault("--forceUnlinkedDFG=1", "--watchdog=1000", "--watchdog-exception-ok", "--useFTLJIT=0")

function test(value)
{
    return value * value;
}
noInline(test);

for (var i = 0;; ++i)
    test(i);
