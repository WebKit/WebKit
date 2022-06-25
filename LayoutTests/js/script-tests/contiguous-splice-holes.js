description("Tests array.splice behavior for contiguous storage with holes.");

// Contiguous splice w/ holes.
var trickyIndex = 9;

var a = new Array(10);
for (var i = 0; i < a.length; ++i) {
    if (i === trickyIndex)
        continue;
    a[i] = i;
}
a.splice(0, 1);
for (var i = 0; i < a.length; ++i) {
    if (i === trickyIndex - 1) {
        shouldBe("a.hasOwnProperty('" + i + "')", "false");
        continue;
    }
    shouldBe("a[" + i + "]", "" + (i + 1));
}
