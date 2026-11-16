//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// This benchmarks quantified parentheses whose body can match an empty
// string, e.g. /(){3}/, /(?:){5}/, /(a?){3}/. The Yarr JIT compiles these
// patterns and then bails to the interpreter at runtime via the
// empty-match-detection branch in ParenthesesSubpattern[FixedCount]End.
// This microbenchmark guards against regressions in either the JIT
// compilation path or the interpreter fallback for the empty-body case.

(function() {
    var result = 0;
    var n = 200000;

    var str1 = "abc";
    var str2 = "";
    var str3 = "aaa";
    var str4 = "aaab";
    var str5 = "xx";

    // Pure empty body — JIT bails to interpreter on the first iteration.
    var re1 = /(){3}/;             // capturing empty
    var re2 = /(?:){3}/;           // non-capturing empty
    var re3 = /((?:)){3}/;         // capturing of empty alternative

    // Optional / alternation that matches empty for some inputs.
    var re4 = /(a?){3}/;           // optional content
    var re5 = /(|x){3}/;           // empty alternation
    var re6 = /(x|){3}/;           // empty alternation

    // Empty inner inside a non-empty outer.
    var re7 = /(a()){3}/;          // outer non-empty, inner empty

    // Empty paren followed by capturing content.
    var re8 = /(){2}(a)b/;

    for (var i = 0; i < n; ++i) {
        if (re1.exec(str1))
            ++result;
        if (re2.exec(str1))
            ++result;
        if (re3.exec(str1))
            ++result;
        if (re4.exec(str2))
            ++result;
        if (re4.exec(str3))
            ++result;
        if (re5.exec(str2))
            ++result;
        if (re6.exec(str5))
            ++result;
        if (re7.exec(str4))
            ++result;
        if (re8.exec("ab"))
            ++result;
    }

    if (result != n * 9)
        throw "Error: bad result: " + result;
})();
