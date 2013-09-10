function foo(x) {
    switch (x) {
    case "aaa": return 1;
    case "aab": return 2;
    case "aac": return 3;
    case "baaa": return 4;
    case "baab": return 5;
    case "baac": return 6;
    case "caaaa": return 7;
    case "caaab": return 8;
    case "caaac": return 9;
    default: return 10;
    }
}

var strings = ["aaa", "aab", "aac", "baaa", "baab", "baac", "caaaa", "caaab", "caaac", "aad", "baad", "caaad", "d", "daa"];

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(strings[i % strings.length]);

if (result != 6785696)
    throw "Bad result: " + result;
