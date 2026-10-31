var source = "let x1 = 0x1F + foo(12, bar) // c\n".repeat(200);
source.charCodeAt(0);

function test(source) {
    var count = 0;
    var index = 0;
    while (index < source.length) {
        var rest = source.slice(index);
        if (rest.startsWith("0x")) {
            index += 4;
            count++;
        } else if (rest.startsWith("//"))
            index += 4;
        else if (rest.startsWith("let"))
            index += 3;
        else if (rest.startsWith("foo("))
            index += 4;
        else
            index++;
    }
    return count;
}
noInline(test);

for (var i = 0; i < 1000; ++i) {
    if (test(source) !== 200)
        throw new Error("bad value");
}
