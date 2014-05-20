description("Tests array.splice behavior for contiguous storage with no holes.");

var a = new Array(10);
for (var i = 0; i < a.length; ++i) {
    a[i] = i;
}
var startIndex = 3;
a.splice(startIndex, 1);
for (var i = 0; i < startIndex; ++i) {
    shouldBe("a[" + i + "]", "" + i);
}

for (var i = startIndex; i < a.length; ++i) {
    shouldBe("a[" + i + "]", "" + (i + 1));
}
