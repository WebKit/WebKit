// [ARM] stress/cached-prototype-setter.js.no-llint fails intermittently on Aarch64 Linux
// https://bugs.webkit.org/show_bug.cgi?id=142277
//@ skip if $architecture == "arm64" and $hostOS == "linux"

(function() {
    var xSetterCalled = false;

    function MyConstructor()
    {
        this.x = 1;
    }
    
    new MyConstructor;
    new MyConstructor;
    function setter() {
        xSetterCalled = true;
    }
    Object.prototype.__defineSetter__("x", setter);
    new MyConstructor;

    if (!xSetterCalled)
        throw new Error("FAIL: 'x' setter was not called.");
})();

(function() {
    var xSetterCalled = false;

    function makeO()
    {
        var o = { };
        o.x = 1;
        return o;
    }

    makeO();
    makeO();
    function setter(x) {
        xSetterCalled = true;
    }
    Object.prototype.__defineSetter__("x", setter);
    makeO();

    if (!xSetterCalled)
        throw new Error("FAIL: 'x' setter was not called.");
})();
