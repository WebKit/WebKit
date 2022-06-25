//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(x) {
    switch (x) {
    case "a": return 1;
    case "ab": return 2;
    case "abc": return 3;
    case "abcd": return 4;
    default: return 10;
    }
}

var strings = ["a", "ab", "abc", "abcd", "abcde"];

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(strings[i % strings.length]);

if (result != 4000000)
    throw "Bad result: " + result;
