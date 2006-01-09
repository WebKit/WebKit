description(
"This tests that propertyIsEnumerable works according to the ECMA spec."
);

a = new Array();
a.foo='bar'

shouldBeFalse("a.propertyIsEnumerable('length')");
shouldBeTrue("a.propertyIsEnumerable ('foo')");
shouldBeFalse("a.propertyIsEnumerable ('non-existant')");

var successfullyParsed = true;
