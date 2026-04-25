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
    
    var foo = async () => {
        shouldBe(bar(), null);
    }
    
    foo();
})();
