function foo(x) {
    switch (x) {
    case "fooa": return 1;
    case "fooab": return 2;
    case "fooabc": return 3;
    case "fooabcd": return 4;
    case "fooabcde": return 5;
    case "fooabcdef": return 6;
    case "fooabcdefg": return 7;
    case "fooabcdefgh": return 8;
    case "fooabcdefghi": return 9;
    case "fooabcdefghij": return 10;
    case "fooabcdefghijk": return 11;
    case "fooabcdefghijkl": return 12;
    case "fooabcdefghijklm": return 13;
    case "fooabcdefghijklmn": return 14;
    default: return 15;
    }
}

var strings = ["a", "ab", "abc", "abcd", "abcde", "abcdef", "abcdefg", "abcdefgh", "abcdefghi", "abcdefghij", "abcdefghijk", "abcdefghijkl", "abcdefghijklm", "abcdefghijklmn", "abcdefghijklmno"];
for (var i = 0; i < strings.length; ++i)
    strings[i] = "foo" + strings[i];

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(strings[i % strings.length]);

if (result != 7999975)
    throw "Bad result: " + result;
