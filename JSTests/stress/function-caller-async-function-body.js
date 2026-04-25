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
    
    async function foo()
    {
        shouldBe(bar(), null);
    }
    
    foo();
})();
