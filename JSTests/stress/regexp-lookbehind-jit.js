function shouldBe(actual, expected) {
    actual = JSON.stringify(actual, (key, value) => value === undefined ? "<undefined>" : value);
    expected = JSON.stringify(expected, (key, value) => value === undefined ? "<undefined>" : value);
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function matchOf(re, string) {
    let match = re.exec(string);
    return match ? [match.index, ...match] : null;
}

shouldBe(matchOf(/(?<=x)a/, "xa"), [1, "a"]);
shouldBe(matchOf(/(?<=x)a/, "ya"), null);
shouldBe(matchOf(/(?<=x)a/, "a"), null);
shouldBe(matchOf(/(?<!x)a/, "ya"), [1, "a"]);
shouldBe(matchOf(/(?<!x)a/, "xa"), null);
shouldBe(matchOf(/(?<!x)a/, "a"), [0, "a"]);
shouldBe(matchOf(/(?<=abc)d/, "abcd"), [3, "d"]);
shouldBe(matchOf(/(?<=abc)d/, "xbcd"), null);
shouldBe(matchOf(/(?<=abc)d/, "bcd"), null);
shouldBe(matchOf(/(?<=)a/, "a"), [0, "a"]);
shouldBe(matchOf(/(?<!)a/, "a"), null);
shouldBe(matchOf(/a(?<=a)/, "a"), [0, "a"]);
shouldBe(matchOf(/a(?<=b)/, "a"), null);
shouldBe(matchOf(/a(?<=xa)b/, "xab"), [1, "ab"]);
shouldBe(matchOf(/a(?<=xa)b/, "yab"), null);
shouldBe(matchOf(/ab(?<=xab)/, "xab"), [1, "ab"]);
shouldBe(matchOf(/(?<=a{3})b/, "aaab"), [3, "b"]);
shouldBe(matchOf(/(?<=a{3})b/, "aab"), null);
shouldBe(matchOf(/(?<=xa{2})b/, "xaab"), [3, "b"]);
shouldBe(matchOf(/(?<=xa{2})b/, "yaab"), null);
shouldBe(matchOf(/(?<=\d{3})x/, "123x"), [3, "x"]);
shouldBe(matchOf(/(?<=\d{3})x/, "12x"), null);
shouldBe(matchOf(/(?<=[ab][cd])x/, "adx"), [2, "x"]);
shouldBe(matchOf(/(?<=[ab][cd])x/, "dax"), null);
shouldBe(matchOf(/(?<=\bfoo)bar/, "foobar"), [3, "bar"]);
shouldBe(matchOf(/(?<=\bfoo)bar/, "xfoobar"), null);
shouldBe(matchOf(/(?<=\Bfoo)bar/, "xfoobar"), [4, "bar"]);
shouldBe(matchOf(/(?<=o\b)bar/, "foo bar"), null);
shouldBe(matchOf(/(?<=o\b.)bar/, "foo bar"), [4, "bar"]);
shouldBe(matchOf(/(?<=\b)bar/, " bar"), [1, "bar"]);
shouldBe(matchOf(/(?<=\B)ar/, "bar"), [1, "ar"]);
shouldBe(matchOf(/(?<=\b)ar/, "bar"), null);
shouldBe(matchOf(/(?<!\b)bar/, " bar"), null);

shouldBe(matchOf(/(?<=ab|xyz)q/, "abq"), [2, "q"]);
shouldBe(matchOf(/(?<=ab|xyz)q/, "xyzq"), [3, "q"]);
shouldBe(matchOf(/(?<=ab|xyz)q/, "yzq"), null);
shouldBe(matchOf(/(?<=xyz|ab)q/, "abq"), [2, "q"]);
shouldBe(matchOf(/(?<=xyz|ab)q/, "xyzq"), [3, "q"]);
shouldBe(matchOf(/(?<=a|bc|def)q/, "defq"), [3, "q"]);
shouldBe(matchOf(/(?<=a|bc|def)q/, "efq"), null);
shouldBe(matchOf(/(?<=(?:ab|x)(?:cd|y))q/, "abyq"), [3, "q"]);
shouldBe(matchOf(/(?<=(?:ab|x)(?:cd|y))q/, "xcdq"), [3, "q"]);
shouldBe(matchOf(/(?<=(?:ab|x)(?:cd|y))q/, "acdq"), null);
shouldBe(matchOf(/(?<=x(?:ab|a)b)q/, "xabq"), [3, "q"]);
shouldBe(matchOf(/(?<=x(?:a|ab)b)q/, "xabq"), [3, "q"]);
shouldBe(matchOf(/(?<=x(?:a|ab)b)q/, "xabbq"), [4, "q"]);

shouldBe(matchOf(/(?<=a+)b/, "aaab"), [3, "b"]);
shouldBe(matchOf(/(?<=a+)b/, "b"), null);
shouldBe(matchOf(/(?<=ba*)c/, "bc"), [1, "c"]);
shouldBe(matchOf(/(?<=ba*)c/, "baaac"), [4, "c"]);
shouldBe(matchOf(/(?<=ba*)c/, "aaac"), null);
shouldBe(matchOf(/(?<=ba*?)c/, "baaac"), [4, "c"]);
shouldBe(matchOf(/(?<=ba+?)c/, "bc"), null);
shouldBe(matchOf(/(?<=ba?)c/, "bac"), [2, "c"]);
shouldBe(matchOf(/(?<=ba?)c/, "baac"), null);
shouldBe(matchOf(/(?<=ba??)c/, "bac"), [2, "c"]);
shouldBe(matchOf(/(?<=b[a-z]*)c/, "bxyc"), [3, "c"]);
shouldBe(matchOf(/(?<=b[a-z]*?)c/, "bxyc"), [3, "c"]);
shouldBe(matchOf(/(?<=b\d{2,4})c/, "b12345c"), null);
shouldBe(matchOf(/(?<=b\d{2,4})c/, "b1234c"), [5, "c"]);
shouldBe(matchOf(/(?<=b\d{2,4})c/, "b1c"), null);
shouldBe(matchOf(/(?<=b\d{2,4}?)c/, "b123c"), [4, "c"]);
shouldBe(matchOf(/(?<=aa+a)b/, "aab"), null);
shouldBe(matchOf(/(?<=aa+a)b/, "aaab"), [3, "b"]);
shouldBe(matchOf(/(?<=ba+ab)c/, "baabc"), [4, "c"]);
shouldBe(matchOf(/(?<=ba+ab)c/, "babc"), null);
shouldBe(matchOf(/(?<=b.*)c/, "bxxc"), [3, "c"]);
shouldBe(matchOf(/(?<=b.*)c/, "xxc"), null);
shouldBe(matchOf(/(?<=^.*b.*)c/, "xbxc"), [3, "c"]);
shouldBe(matchOf(/(?<!^.*b.*)c/, "xbxc"), null);
shouldBe(matchOf(/(?<=x(?:ab)?)c/, "xabc"), [3, "c"]);
shouldBe(matchOf(/(?<=x(?:ab)?)c/, "xc"), [1, "c"]);
shouldBe(matchOf(/(?<=x(?:ab)?)c/, "xbc"), null);
shouldBe(matchOf(/(?<=x(?:ab)??)c/, "xabc"), [3, "c"]);
shouldBe(matchOf(/(?<! ya(?:yandex)?)search/, "yasearch"), [2, "search"]);
shouldBe(matchOf(/(?<! ya(?:yandex)?)search/, " yasearch"), null);
shouldBe(matchOf(/(?<! ya(?:yandex)?)search/, " yayandexsearch"), null);
shouldBe(matchOf(/(?<! ya(?:yandex)?)search/, "search"), [0, "search"]);

shouldBe(matchOf(/(?<=(a)(b))c/, "abc"), [2, "c", "a", "b"]);
shouldBe(matchOf(/(?<=(a+))b/, "xaaab"), [4, "b", "aaa"]);
shouldBe(matchOf(/(?<=(a+?))b/, "xaaab"), [4, "b", "a"]);
shouldBe(matchOf(/(?<=(\d+)(\d+))x/, "1234x"), [4, "x", "1", "234"]);
shouldBe(matchOf(/(?<=(\d+?)(\d+?))x/, "1234x"), [4, "x", "3", "4"]);
shouldBe(matchOf(/(?<=x(ab|a)(b?))c/, "xabc"), [3, "c", "a", "b"]);
shouldBe(matchOf(/(?<=(ab)?x)c/, "abxc"), [3, "c", "ab"]);
shouldBe(matchOf(/(?<=(ab)?x)c/, "bxc"), [2, "c", undefined]);
shouldBe(matchOf(/(?<=(ab)??x)c/, "abxc"), [3, "c", undefined]);
shouldBe(/(?<=(?<n>a)b)c/.exec("abc").groups.n, "a");
shouldBe(matchOf(/(?<!(a))b/, "cb"), [1, "b", undefined]);
shouldBe(matchOf(/(?<!(a)c)b/, "acb"), null);
shouldBe(matchOf(/(a)(?<=(a))b/, "ab"), [0, "ab", "a", "a"]);
shouldBe(matchOf(/(?<=(a))bz|(?<=(a))b/, "ab"), [1, "b", undefined, "a"]);
shouldBe(matchOf(/(?:(?<=(a))b)+z|b/, "abb"), [1, "b", undefined]);
shouldBe(matchOf(/(?<=(a)|(x))b/, "xb"), [1, "b", undefined, "x"]);
shouldBe(matchOf(/(?<=(a)|(x))b/, "ab"), [1, "b", "a", undefined]);

shouldBe(matchOf(/(?<=(?<=a)b)c/, "abc"), [2, "c"]);
shouldBe(matchOf(/(?<=(?<=a)b)c/, "xbc"), null);
shouldBe(matchOf(/(?<=b(?<=ab))c/, "abc"), [2, "c"]);
shouldBe(matchOf(/(?<=b(?<=ab))c/, "xbc"), null);
shouldBe(matchOf(/(?<=(?<!a)b)c/, "xbc"), [2, "c"]);
shouldBe(matchOf(/(?<=(?<!a)b)c/, "abc"), null);
shouldBe(matchOf(/(?<=x(?<=(x))y)c/, "xyc"), [2, "c", "x"]);
shouldBe(matchOf(/(?=a(?<=([bx])a))./, "xa"), [1, "a", "x"]);
shouldBe(matchOf(/(?=.(?<=([bx])a))./, "xa"), [1, "a", "x"]);
shouldBe(matchOf(/(?=.(?<=([bx])a))./, "ya"), null);
shouldBe(matchOf(/q(?=ab(?<=q(a)b))/, "qab"), [0, "q", "a"]);
shouldBe(matchOf(/(?!ab(?<=xab)).b/, "xabyab"), [4, "ab"]);

shouldBe(matchOf(/(?<=^)a/, "a"), [0, "a"]);
shouldBe(matchOf(/(?<=^)a/, "ba"), null);
shouldBe(matchOf(/(?<!^)a/, "a"), null);
shouldBe(matchOf(/(?<!^)a/, "ba"), [1, "a"]);
shouldBe(matchOf(/(?<=^x)y/, "xy"), [1, "y"]);
shouldBe(matchOf(/(?<=^x)y/, "axy"), null);
shouldBe(matchOf(/(?<=^a*)b/, "aab"), [2, "b"]);
shouldBe(matchOf(/(?<=^a*)b/, "xaab"), null);
shouldBe(matchOf(/(?<=x^)y/, "xy"), null);
shouldBe(matchOf(/(?<=x$)/, "x"), [1, ""]);
shouldBe(matchOf(/(?<=x$)/, "xy"), null);
shouldBe(matchOf(/(?<=$x)/, "x"), null);
shouldBe(matchOf(/(?<=a*$)/, "aa"), [2, ""]);
shouldBe(matchOf(/x(?<=^x$)/, "x"), [0, "x"]);
shouldBe(matchOf(/x(?<=^x$)/, "xx"), null);
shouldBe(matchOf(/(?<=^a)b/m, "xa\nab"), [4, "b"]);
shouldBe(matchOf(/(?<=^a)b/m, "xab"), null);
shouldBe(matchOf(/(?<=^)a/m, "\na"), [1, "a"]);
shouldBe(matchOf(/(?<=a$)/m, "a\nb"), [1, ""]);
shouldBe(matchOf(/(?<=a$\n)b/m, "a\nb"), [2, "b"]);
shouldBe(matchOf(/(?<=a$.)b/m, "a\nb"), null);
shouldBe(matchOf(/(?<=a$.)b/ms, "a\nb"), [2, "b"]);
shouldBe(matchOf(/(?<=$)b/m, "a\nb"), null);
shouldBe(matchOf(/(?<=^[^]?)b/m, "a\nb"), [2, "b"]);

{
    let re = /(?<=ab)c/g;
    let indices = [];
    let match;
    while ((match = re.exec("abcabc")) !== null)
        indices.push(match.index);
    shouldBe(indices, [2, 5]);

    let sticky = /(?<=ab)c/y;
    sticky.lastIndex = 2;
    shouldBe(matchOf(sticky, "abc"), [2, "c"]);
    sticky.lastIndex = 1;
    shouldBe(matchOf(sticky, "abc"), null);

    shouldBe("abcabc".replace(/(?<=b)c/g, "X"), "abXabX");
    shouldBe("aaa".replace(/(?<=a)/g, "-"), "a-a-a-");
    shouldBe("aaa".replace(/(?<!a)/g, "-"), "-aaa");
    shouldBe("1234567".replace(/(?<=\d)(?=(?:\d{3})+$)/g, ","), "1,234,567");
    shouldBe("abcabc".split(/(?<=b)/), ["ab", "cab", "c"]);
}

shouldBe(matchOf(/(?<=ab)c/i, "ABC"), [2, "C"]);
shouldBe(matchOf(/(?<=k)c/i, "Kc"), [1, "c"]);
shouldBe(matchOf(/(?<=a[bx]+)c/i, "ABXc"), [3, "c"]);
shouldBe(matchOf(/(?<=\u00e9)x/, "\u00e9x"), [1, "x"]);
shouldBe(matchOf(/(?<=\u0100)x/, "\u0100x"), [1, "x"]);
shouldBe(matchOf(/(?<=\u0100+)x/, "\u0100\u0100x"), [2, "x"]);
shouldBe(matchOf(/(?<=ab)c/, "\u0100abc"), [3, "c"]);
shouldBe(matchOf(/(?<=\u0100)x/, "ax"), null);
shouldBe(matchOf(/(?<=[\u0100-\u0200]{2})x/, "a\u0100\u0180x"), [3, "x"]);
shouldBe(matchOf(/(?<=\ud83d)\ude00/, "\ud83d\ude00"), [1, "\ude00"]);

shouldBe(matchOf(/.*(?<=a)/, "xxaxx"), [0, "xxa"]);
shouldBe(matchOf(/.*(?<=a)x/, "xxaxx"), [0, "xxax"]);
shouldBe(matchOf(/.+?(?<=a)/, "xxaxa"), [0, "xxa"]);
shouldBe(matchOf(/[a-z]+(?<!q)!/, "aq!ab!"), [3, "ab!"]);
shouldBe(matchOf(/(?:ab|a)(?<=a)c/, "abac"), [2, "ac"]);
shouldBe(matchOf(/(a|ab)(?<=b)c/, "abc"), [0, "abc", "ab"]);
shouldBe(matchOf(/^(?:foo|bar)(?<=r)$/, "bar"), [0, "bar"]);
shouldBe(matchOf(/^(?:foo|bar)(?<=r)$/, "foo"), null);
shouldBe(matchOf(/x*(?<=xx)y/, "xxxy"), [0, "xxxy"]);
shouldBe(matchOf(/x*(?<=xx)y/, "xy"), null);
shouldBe(matchOf(/(?<=a.{6})b/, "a123456b"), [7, "b"]);
shouldBe(matchOf(new RegExp("(?<=a.{200})b"), "a" + "1".repeat(200) + "b"), [201, "b"]);
shouldBe(matchOf(new RegExp("(?<=a.{200})b"), "1".repeat(201) + "b"), null);
shouldBe(matchOf(new RegExp("(?<=a" + "\\d".repeat(50) + ")b"), "a" + "1".repeat(50) + "b"), [51, "b"]);

{
    let re = /(?<! cu)bot|(?<!lib)http|(?<! ya(?:yandex)?)search|crawler|spider/;
    let lines = ["Googlebot/2.1", "a cubot toy", "libhttp client", "http agent", "yandexsearch", " yasearch", "crawler", "spiderman"];
    shouldBe(lines.map(line => re.test(line)), [true, false, false, true, true, false, true, true]);
}

shouldBe(matchOf(/a(?<!x)(^)?$/, "xa"), [1, "a", undefined]);
shouldBe(matchOf(/(?<=y)a(^)?$/, "ya"), [1, "a", undefined]);
shouldBe(matchOf(/(?<!q)b(^)*c/, "abc"), [1, "bc", undefined]);
shouldBe(matchOf(/\w(?<=\d)(^)?/, "a1"), [1, "1", undefined]);
shouldBe("qxaqxa".split(/a(?<!x)(^)?$/), ["qxaqx", undefined, ""]);

shouldBe(matchOf(/(?<=(a)\1)b/, "aab"), [2, "b", "a"]);
shouldBe(matchOf(/(?<=(a)\1)b/, "xab"), [2, "b", "a"]);
shouldBe(matchOf(/(?<=\1(a))b/, "xab"), null);
shouldBe(matchOf(/(?<=\1(a))b/, "aab"), [2, "b", "a"]);
shouldBe(matchOf(/(a)x(?<=\1x)b/, "axb"), [0, "axb", "a"]);
shouldBe(matchOf(/(?<=(?:ab)+)c/, "ababc"), [4, "c"]);
shouldBe(matchOf(/(?<=(?:ab)+)c/, "xc"), null);
shouldBe(matchOf(/(?<=(?:ab){2})c/, "ababc"), [4, "c"]);
shouldBe(matchOf(/(?<=(?:ab){2})c/, "xabc"), null);
shouldBe(matchOf(/(?<=(?:ab){1,2}x)c/, "abxc"), [3, "c"]);
shouldBe(matchOf(/(?<=(?=a)a)b/, "ab"), [1, "b"]);
shouldBe(matchOf(/(?<=(?=a)a)b/, "cb"), null);
shouldBe(matchOf(/(?<=(?!a).)b/, "cb"), [1, "b"]);
shouldBe(matchOf(/(?<=(?!a).)b/, "ab"), null);
shouldBe(matchOf(/(?<=\u{1F600})x/u, "\u{1F600}x"), [2, "x"]);
shouldBe(matchOf(/(?<=\u{1F600})x/u, "yx"), null);
shouldBe(matchOf(/(?<=.)x/u, "\u{1F600}x"), [2, "x"]);
