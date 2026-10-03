function indexOfNewline(string) {
    return string.indexOf("\n");
}
noInline(indexOfNewline);

function includesTab(string) {
    return string.includes("\t");
}
noInline(includesTab);

var line = "The quick brown fox jumps over the lazy dog. ";
var strings = [
    line.repeat(3) + "\n" + line,
    line.repeat(4),
    line.repeat(5) + "\t" + line + "\n",
    line + "\n" + line.repeat(3),
    line.repeat(6) + "\n",
    line.repeat(3) + "\t",
    line.repeat(7),
    line.repeat(2) + "\t" + line.repeat(2) + "\n" + line,
].map((string) => JSON.parse(JSON.stringify(string)));

var result = 0;
for (var i = 0; i < 1e6; ++i) {
    var string = strings[i & 7];
    result += indexOfNewline(string);
    if (includesTab(string))
        result++;
}
if (result !== 112750000)
    throw new Error("bad result: " + result);
