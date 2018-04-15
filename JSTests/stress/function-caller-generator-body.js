//@ runDefault

(function thingy() {
    function bar()
    {
        return bar.caller;
    }
    
    function* foo()
    {
        bar();
    }
    
    var ok = false;
    try {
        foo().next();
        ok = true;
    } catch (e) {
        if (e.toString() != "TypeError: Function.caller used to retrieve generator body")
            throw "Error: bad error: " + e;
    }
    if (ok)
        throw "Error: did not throw error";
})();
