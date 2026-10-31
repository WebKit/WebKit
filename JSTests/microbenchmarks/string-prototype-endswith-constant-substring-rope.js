var source = "index.js main.css logo.png app.json ".repeat(200);
source.charCodeAt(0);

function test(source) {
    var count = 0;
    for (var end = 8; end < source.length; ++end) {
        var head = source.slice(0, end);
        if (head.endsWith(".js") || head.endsWith(".css") || head.endsWith(".json"))
            count++;
    }
    return count;
}
noInline(test);

for (var i = 0; i < 1000; ++i) {
    if (test(source) !== 800)
        throw new Error("bad value");
}
