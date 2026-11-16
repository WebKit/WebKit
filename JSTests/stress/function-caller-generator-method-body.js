//@ runDefault

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

(function thingy() {
    function bar()
    {
        return bar.caller;
    }

    class C {
        *foo()
        {
            shouldBe(bar(), null);
        }
    }
    new C().foo().next();
})();
