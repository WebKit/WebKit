description("Test to ensure correct behaviour of splice in array storage mode with indexed properties in the prototype chain.");

// Array storage shift holes require special behavior.
var trickyIndex = 6;
Object.prototype[trickyIndex] = trickyIndex;

var a = new Array(10);
for (var i = 0; i < a.length; ++i) {
    if (i === trickyIndex)
        continue;
    a[i] = i;
}

a.shift(); // Converts to array storage mode.
var startIndex = 3;
a.splice(startIndex, 1);

for (var i = 0; i < startIndex; ++i)
    shouldBe("a[" + i + "]", "" + (i + 1));

for (var i = startIndex; i < a.length; ++i)
    shouldBe("a[" + i + "]", "" + (i + 2));

// Array storage shift holes require special behavior, but there aren't any holes.
var b = new Array(10);
for (var i = 0; i < b.length; ++i)
    b[i] = i;

b.shift(); // Converts to array storage mode.
b.splice(startIndex, 1);

for (var i = 0; i < startIndex; ++i)
    shouldBe("b[" + i + "]", "" + (i + 1));

for (var i = startIndex; i < b.length; ++i)
    shouldBe("b[" + i + "]", "" + (i + 2));
