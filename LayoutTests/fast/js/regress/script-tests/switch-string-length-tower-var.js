function foo(x) {
    switch (x) {
    case "fooa": return 1;
    case "fooab": return 2;
    case "fooabc": return 3;
    case "fooabcd": return 4;
    default: return 10;
    }
}

var strings = ["a", "ab", "abc", "abcd", "abcde"];
for (var i = 0; i < strings.length; ++i)
    strings[i] = "foo" + strings[i];

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(strings[i % strings.length]);

if (result != 4000000)
    throw "Bad result: " + result;
