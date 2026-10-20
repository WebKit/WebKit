var strings = ["foo", "bar", "xfoooy", "baz", "fo", "f"];

function replacer(match) {
    return "0";
}

function test(string) {
    return string.replaceAll("o", "0") + string.replaceAll("o", replacer) + string.replace("o", replacer);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test(strings[i % 6]);
