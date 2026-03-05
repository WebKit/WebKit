//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// This benchmarks non-capturing parentheses with fixed count quantifiers like (?:x){5}.
// These patterns use the ParenthesesSubpatternFixedCount JIT optimization.

(function() {
    var result = 0;
    var n = 500000;

    // Test strings that will match
    var str1 = "aaaaabbbbb";          // for (?:a){5}
    var str2 = "abcabcabc";           // for (?:abc){3}
    var str3 = "xyxyxyxy";            // for (?:xy){4}
    var str4 = "testabcdefabcdefend"; // for (?:abcdef){2}

    // Patterns with non-capturing fixed count groups
    var re1 = /(?:a){5}/;
    var re2 = /(?:abc){3}/;
    var re3 = /(?:xy){4}/;
    var re4 = /(?:abcdef){2}/;

    for (var i = 0; i < n; ++i) {
        if (re1.exec(str1))
            ++result;
        if (re2.exec(str2))
            ++result;
        if (re3.exec(str3))
            ++result;
        if (re4.exec(str4))
            ++result;
    }

    if (result != n * 4)
        throw "Error: bad result: " + result;
})();
