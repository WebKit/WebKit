function foo(x) {
    switch (x) {
    case "aaa": return 1;
    case "aab": return 2;
    case "aac": return 3;
    case "aba": return 10;
    case "abb": return 20;
    case "abc": return 30;
    case "baaa": return 4;
    case "baab": return 5;
    case "baac": return 6;
    case "bbaa": return 40;
    case "bbab": return 50;
    case "bbac": return 60;
    case "bbba": return 400;
    case "bbbb": return 500;
    case "bbbc": return 600;
    case "caaaa": return 7;
    case "caaab": return 8;
    case "caaac": return 9;
    case "cbaaa": return 70;
    case "cbaab": return 80;
    case "cbaac": return 90;
    case "cbbaa": return 700;
    case "cbbab": return 800;
    case "cbbac": return 900;
    case "cbbba": return 7000;
    case "cbbbb": return 8000;
    case "cbbbc": return 9000;
    case "dbbba": return 70000;
    case "dbbbb": return 80000;
    case "dbbbc": return 90000;
    case "ebaaa": return 400000;
    case "ebaab": return 500000;
    case "ebaac": return 600000;
    default: return 10;
    }
}

function make(pre, post) { return pre + "a" + post; }

var strings = [make("a", "a"), make("a", "b"), make("a", "c"), make("b", "aa"), make("b", "ab"), make("b", "ac"), make("c", "aaa"), make("c", "aab"), make("c", "aac"), make("a", "d"), make("b", "ad"), make("c", "aad"), "d", make("d", "a")];

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(strings[i % strings.length]);

if (result != 6785696)
    throw "Bad result: " + result;
