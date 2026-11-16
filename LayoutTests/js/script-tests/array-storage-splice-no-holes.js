description("Tests array.splice behavior for array storage with no holes.");

// Array storage splice w/o holes.
var a = new Array(10);
for (var i = 0; i < a.length; ++i)
    a[i] = i;

a.shift(); // Converts to array storage mode.

var startIndex = 4;
a.splice(startIndex, 1);

for (var i = 0; i < startIndex; ++i)
    shouldBe("a[" + i + "]", "" + (i + 1));

for (var i = startIndex; i < a.length; ++i)
    shouldBe("a[" + i + "]", "" + (i + 2));
