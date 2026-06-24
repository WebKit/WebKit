//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// This benchmarks capturing parentheses with fixed-count quantifiers whose
// body has a single alternative and contains nothing backtrackable, e.g.
// /([0-9a-fA-F]){12}/ or /([0-9a-fA-F]{4}\.){2}/. These now route to the
// ParenthesesSubpatternFixedCount fast path that no longer allocates a
// ParenContext per iteration.

(function() {
    var result = 0;
    var n = 500000;

    // Test strings that will match
    var str1 = "0123456789ab";        // for ([0-9a-fA-F]){12}
    var str2 = "abcd.efff.0011";      // for ([0-9a-fA-F]{4}\.){2}([0-9a-fA-F]{4})
    var str3 = "aaa";                 // for ([a-z]){3}
    var str4 = "abcabc";              // for ([a-z]{3}){2}

    // Test strings that will fail (exercise the abort/backtrack path).
    var fail1 = "0123456789ax";
    var fail2 = "abcd.efgh.0011";

    var re1 = /([0-9a-fA-F]){12}/;
    var re2 = /([0-9a-fA-F]{4}\.){2}([0-9a-fA-F]{4})/;
    var re3 = /([a-z]){3}/;
    var re4 = /([a-z]{3}){2}/;

    for (var i = 0; i < n; ++i) {
        if (re1.exec(str1))
            ++result;
        if (re2.exec(str2))
            ++result;
        if (re3.exec(str3))
            ++result;
        if (re4.exec(str4))
            ++result;
        if (re1.exec(fail1) === null)
            ++result;
        if (re2.exec(fail2) === null)
            ++result;
    }

    if (result != n * 6)
        throw "Error: bad result: " + result;
})();
