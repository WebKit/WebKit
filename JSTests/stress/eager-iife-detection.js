//@ requireOptions("--useEagerIIFEParsing=true", "--useDollarVM=1")

function assertCounts({detected, succeeded}, src) {
    const initialDetected = $vm.iifeDetectionCount();
    const initialSucceeded = $vm.iifeSuccessCount();
    eval(src);
    const actualDetected = $vm.iifeDetectionCount() - initialDetected;
    const actualSucceeded = $vm.iifeSuccessCount() - initialSucceeded;
    if (actualDetected !== detected || actualSucceeded !== succeeded)
        throw new Error(`${JSON.stringify(src)}: expected {detected:${detected}, succeeded:${succeeded}}, got {detected:${actualDetected}, succeeded:${actualSucceeded}}`);
}

// === True IIFEs — every detection succeeds. ===

assertCounts({detected: 1, succeeded: 1}, "(function() { return 1; })();");
assertCounts({detected: 1, succeeded: 1}, "(function() { return 2; }());");
assertCounts({detected: 1, succeeded: 1}, "!function() { return 3; }();");
assertCounts({detected: 1, succeeded: 1}, "(function(x) { return x; })(1);");
assertCounts({detected: 1, succeeded: 1}, "(function(x, y) { return x + y; })(1, 2);");
assertCounts({detected: 1, succeeded: 1}, "(function(x, y, z) { return x + y + z; })(1, 2, 3);");
assertCounts({detected: 1, succeeded: 1}, "!function(z) { return z; }(4);");


// === Not IIFEs — neither detected nor eager-parsed. ===

// Function expression passed as call argument; the `(` opens a call
// arg list, not a parenthesised expression.
assertCounts({detected: 0, succeeded: 0}, "function callWith(fn) { return fn(); } callWith(function() { return 4; });");

// Object property, array element, variable initializer.
assertCounts({detected: 0, succeeded: 0}, "var obj = { fn: function() { return 5; } };");
assertCounts({detected: 0, succeeded: 0}, "var f = function() { return 6; };");
assertCounts({detected: 0, succeeded: 0}, "var arr = [function() { return 7; }];");

// Function declaration — not even a function expression.
assertCounts({detected: 0, succeeded: 0}, "function decl() { return 8; }");

// `+function()` — the detector only fires for `!`, not other unary
// prefixes.
assertCounts({detected: 0, succeeded: 0}, "+function() { return 10; }();");


// === Nested IIFE — both outer and inner detect and succeed. ===

assertCounts({detected: 2, succeeded: 2},
    "(function() { return (function() { return 14; })(); })();");


// === Empty-body IIFE — detected but does NOT succeed because we are not creating and caching ASTs for those until they are needed.

assertCounts({detected: 1, succeeded: 0}, "(function() {})();");
assertCounts({detected: 1, succeeded: 0}, "!function() {}();");


// === Parenthesised function expression without a following call (false positive - not really an IIFE).

assertCounts({detected: 1, succeeded: 1}, "var g = (function() { return 11; }); g();");
