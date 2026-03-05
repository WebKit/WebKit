//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
// Microbenchmark for backreference patterns in MatchOnly mode (test() method).
// This measures the performance of JIT-compiled backreference matching.

(function() {
    // Warmup and benchmark iterations
    const iterations = 1000000;

    // Test 1: Simple backreference
    let re1 = /(.)\1/;
    let str1_match = "aa";
    let str1_nomatch = "ab";
    for (let i = 0; i < iterations; ++i) {
        re1.test(str1_match);
        re1.test(str1_nomatch);
    }

    // Test 2: Multiple backreferences
    let re2 = /(.)(.)\1\2/;
    let str2_match = "abab";
    let str2_nomatch = "abba";
    for (let i = 0; i < iterations; ++i) {
        re2.test(str2_match);
        re2.test(str2_nomatch);
    }

    // Test 3: Quantified backreference
    let re3 = /(.)\1+/;
    let str3_match = "aaaa";
    let str3_nomatch = "abcd";
    for (let i = 0; i < iterations; ++i) {
        re3.test(str3_match);
        re3.test(str3_nomatch);
    }

    // Test 4: Named capture group
    let re4 = /(?<char>.)\k<char>/;
    let str4_match = "bb";
    let str4_nomatch = "bc";
    for (let i = 0; i < iterations; ++i) {
        re4.test(str4_match);
        re4.test(str4_nomatch);
    }

    // Test 5: Complex pattern (fences-style)
    let re5 = /(`{3,}).*\1/;
    let str5_match = "```code```";
    let str5_nomatch = "```code``";
    for (let i = 0; i < iterations / 10; ++i) {  // Fewer iterations for complex pattern
        re5.test(str5_match);
        re5.test(str5_nomatch);
    }

    // Test 6: Duplicate named capture groups
    let re6 = /(?:(?<x>a)|(?<x>b))\k<x>/;
    let str6_match = "aa";
    let str6_nomatch = "ab";
    for (let i = 0; i < iterations; ++i) {
        re6.test(str6_match);
        re6.test(str6_nomatch);
    }
})();
