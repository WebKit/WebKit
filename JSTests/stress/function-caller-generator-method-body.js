//@ runDefault

(function thingy() {
    function bar()
    {
        return bar.caller;
    }
    
    class C {
        *foo()
        {
            bar();
        }
    }
        
    var ok = false;
    try {
        new C().foo().next();
        ok = true;
    } catch (e) {
        if (e.toString() != "TypeError: Function.caller used to retrieve generator body")
            throw "Error: bad error: " + e;
    }
    if (ok)
        throw "Error: did not throw error";
})();
