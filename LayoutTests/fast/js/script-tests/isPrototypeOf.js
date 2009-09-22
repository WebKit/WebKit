description(
"This tests that isPrototypeOf works according to the ECMA spec."
);

shouldBeFalse("Object.isPrototypeOf('string')");
shouldBeTrue("Array.prototype.isPrototypeOf(new Array())");

function ObjectA() { }
function ObjectB() { }
ObjectB.prototype = new ObjectA();
var objb = new ObjectB();
shouldBeTrue("ObjectA.prototype.isPrototypeOf(objb)");

function ObjectC() {}
ObjectC.prototype = new ObjectB();
var objc = new ObjectC();
shouldBeTrue("ObjectA.prototype.isPrototypeOf(objc)");

var successfullyParsed = true;
