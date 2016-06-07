description("This test ensures that WebKit doesn't crash when the document.createTouchList API is called with non-Touch parameters");

shouldThrow('document.createTouchList(document).item(0)');
shouldThrow('document.createTouchList({"a":1}).item(0)');
shouldThrow('document.createTouchList(new Array(5)).item(0)');
shouldThrow('document.createTouchList("string").item(0)');
shouldThrow('document.createTouchList(null).item(0)');
shouldThrow('document.createTouchList(undefined).item(0)');
shouldNotThrow('document.createTouchList()');
shouldBe('document.createTouchList().length', '0');

var t = document.createTouch(window, document.body, 12341, 60, 65, 100, 105);
var t2 = document.createTouch(window, document.body, 12342, 50, 55, 115, 120);

shouldThrow('document.createTouchList(t, document, t2);');

var tl = document.createTouchList(t, t2);
shouldBe('tl.length','2');
shouldBeNonNull('tl.item(0)');
shouldBeNonNull('tl.item(1)');

isSuccessfullyParsed();
