(function nonInlinedTailCall() {
    function callee() { if (callee.caller != nonInlinedTailCall) throw new Error(); }
    noInline(callee);

    function caller() { "use strict"; return callee(); }

    for (var i = 0; i < 10000; ++i)
        caller();

    function loop(n) { "use strict"; if (n > 0) return loop(n - 1); }
    noInline(loop);

    loop(1000000);
})();

(function inlinedTailCall() {
    function callee() { if (callee.caller != inlinedTailCall) throw new Error(); }
    function caller() { "use strict"; return callee(); }

    for (var i = 0; i < 10000; ++i)
        caller();

    function loop(n) { "use strict"; if (n > 0) return loop(n - 1); }

    loop(1000000);
})();

(function nonInlinedEmulatedTailCall() {
    function emulator() { caller(); }
    function callee() { if (callee.caller != emulator) throw new Error(); }
    noInline(callee);
    function caller() { "use strict"; return callee(); }

    for (var i = 0; i < 10000; ++i)
        emulator();

    function pad(n) { "use strict"; return loop(n); }
    function loop(n) { "use strict"; if (n > 0) return pad(n - 1); }
    noInline(loop);

    loop(1000000);
})();

(function inlinedEmulatedTailCall() {
    function emulator() { caller(); }
    function callee() { if (callee.caller != emulator) throw new Error(); }
    function caller() { "use strict"; return callee(); }

    for (var i = 0; i < 10000; ++i)
        emulator();

    function pad(n) { "use strict"; return loop(n); }
    function loop(n) { "use strict"; if (n > 0) return pad(n - 1); }

    loop(1000000);
})();
