function foo(x) {
    switch (x) {
    case "aa": return 1;
    case "ab": return 2;
    case "ac": return 3;
    case "ad": return 4;
    default: return 10;
    }
}

var strings = ["aa", "ab", "ac", "ad", "ae"];

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(strings[i % strings.length]);

if (result != 4000000)
    throw "Bad result: " + result;
