function spread(string) {
    return [...string];
}
noInline(spread);

var string = "𠮷野家で𩸽と𠀋を食べた😀😃😄😁🍣🍺".repeat(10);
var expectedLength = spread(string).length;
for (var i = 0; i < 1e5; ++i) {
    var result = spread(string);
    if (result.length !== expectedLength)
        throw new Error("bad length: " + result.length);
}
