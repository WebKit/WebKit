description(
"This test checks whether various forms of delete expression are allowed."
);

window.y = 0;

shouldBeTrue('delete x');
window.x = 0;
window.y = 0;
shouldBeTrue('delete window.x');
window.x = 0;
window.y = 0;
shouldBeTrue('delete window["x"]');
window.x = 0;
window.y = 0;
shouldBeTrue('delete (x)');
window.x = 0;
window.y = 0;
shouldBeTrue('delete (window.x)');
window.x = 0;
window.y = 0;
shouldBeTrue('delete (window["x"])');
window.x = 0;
window.y = 0;
shouldBeTrue('(y, delete x)');
window.x = 0;
window.y = 0;
shouldBeTrue('delete ((x))');
window.x = 0;
window.y = 0;
shouldBeTrue('delete ((window.x))');
window.x = 0;
window.y = 0;
shouldBeTrue('delete ((window["x"]))');
window.x = 0;
window.y = 0;
shouldBeTrue('delete (y, x)');
window.x = 0;
window.y = 0;
shouldBeTrue('delete (true ? x : y)');
window.x = 0;
window.y = 0;

shouldBeTrue('delete nonexistent');
shouldBeTrue('delete window.nonexistent');
shouldBeTrue('delete window["nonexistent"]');
shouldBeTrue('delete (nonexistent)');
shouldBeTrue('delete (window.nonexistent)');
shouldBeTrue('delete (window["nonexistent"])');

shouldBeTrue('delete "x"');
shouldBeTrue('delete (2 + 3)');

var mathCos = Math.cos;
delete Math.sin;
Math.tan = null;
// Try deleting & overwriting static properties.
shouldBe("Math.cos", "mathCos");
shouldBe("Math.sin", "undefined");
shouldBe("Math.tan", "null");

var regExpPrototypeCompile = RegExp.prototype.compile;
RegExp.prototype.test = null;
Object.preventExtensions(RegExp.prototype);
delete RegExp.prototype.exec;
// Reverse order of delete & overwrite, tests delete after preventExtensions.
shouldBe("RegExp.prototype.compile", "regExpPrototypeCompile");
shouldBe("RegExp.prototype.exec", "undefined");
shouldBe("RegExp.prototype.test", "null");

// Check that once a property is deleted its name is removed from the property name array.
delete Object.prototype.__defineSetter__;
shouldBe("Object.getOwnPropertyNames(Object.prototype).indexOf('__defineSetter__')", "-1");

delete navigator.appCodeName;
var navigatorPropertyNames = Object.getOwnPropertyNames(navigator);
var expectedPropertyNames = [
    "appName",
    "appVersion",
    "language",
    "userAgent",
    "platform",
    "plugins",
    "mimeTypes",
    "product",
    "productSub",
    "vendor",
    "vendorSub",
    "cookieEnabled",
    "onLine"
];

for (var i = 0; i < expectedPropertyNames.length; ++i)
    shouldBeTrue("navigatorPropertyNames.indexOf('" + expectedPropertyNames[i] +"') == -1");
var navigatorPrototypePropertyNames = Object.getOwnPropertyNames(navigator.__proto__);
for (var i = 0; i < expectedPropertyNames.length; ++i)
    shouldBeTrue("navigatorPrototypePropertyNames.indexOf('" + expectedPropertyNames[i] +"') != -1");
