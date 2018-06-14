//@ runDefault

(function thingy() {
    function bar()
    {
        return bar.caller;
    }
    
    var ok = false;
    var badError = null;
    async function* foo()
    {
        try {
            bar();
            ok = true;
        } catch (e) {
            if (e.toString() != "TypeError: Function.caller used to retrieve generator body")
                badError = e;
        }
    }
    
    foo().next();
    if (ok)
        throw "Error: did not throw error";
    if (badError)
        throw "Bad error: " + badError;
})();
