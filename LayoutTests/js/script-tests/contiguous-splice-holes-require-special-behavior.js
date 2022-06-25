description("Tests array.splice behavior with holey contiguous storage with indexed properties in the prototype chain.");

// Contiguous splice holes require special behavior.
var trickyIndex = 9;
Object.prototype[trickyIndex] = trickyIndex;

var a = new Array(10);
for (var i = 0; i < a.length; ++i) {
    if (i === trickyIndex)
        continue;
    a[i] = i;
}
a.splice(0, 1);
for (var i = 0; i < a.length; ++i) {
    shouldBe("a[" + i + "]", "" + (i + 1));
}
