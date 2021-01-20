description(
"Tests that deleting all properties from an object and then flattening it doesn't cause inconsistencies."
);

var o = {};

for (var i = 0; i < 1000; ++i)
    o["a" + i] = i;

for (var i = 0; i < 1000; ++i)
    delete o["a" + i];

var p = {};
p.__proto__ = o;

var q = {f:42};
o.__proto__ = q;

for (var i = 0; i < 100; ++i)
    shouldBe("p.f", "42");
