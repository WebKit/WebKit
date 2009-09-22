description(
"This tests that propertyIsEnumerable works according to the ECMA spec."
);

a = new Array();
a.foo='bar'

var aVarDecl;
function aFunctionDecl(){}
var global = this;
shouldBeFalse("a.propertyIsEnumerable('length')");
shouldBeTrue("a.propertyIsEnumerable ('foo')");
shouldBeFalse("a.propertyIsEnumerable ('non-existant')");

shouldBeTrue("global.propertyIsEnumerable ('aVarDecl')");
shouldBeTrue("global.propertyIsEnumerable ('aFunctionDecl')");
shouldBeFalse("global.propertyIsEnumerable ('Math')");
shouldBeFalse("global.propertyIsEnumerable ('NaN')");
shouldBeFalse("global.propertyIsEnumerable ('undefined')");

var successfullyParsed = true;
