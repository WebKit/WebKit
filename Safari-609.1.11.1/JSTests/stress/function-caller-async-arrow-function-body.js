//@ runDefault

(function thingy() {
    function bar()
    {
        return bar.caller;
    }
    
    var ok = false;
    var badError = null;
    var foo = async () => {
        try {
            bar();
            ok = true;
        } catch (e) {
            if (e.toString() != "TypeError: Function.caller used to retrieve async function body")
                badError = e;
        }
    }
    
    foo();
    if (ok)
        throw "Error: did not throw error";
    if (badError)
        throw "Bad error: " + badError;
})();
