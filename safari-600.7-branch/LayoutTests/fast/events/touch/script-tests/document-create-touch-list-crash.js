description("This test ensures that WebKit doesn't crash when the document.createTouchList API is called with non-Touch parameters");

shouldBeNull('document.createTouchList(document).item(0)');
shouldBeNull('document.createTouchList({"a":1}).item(0)');
shouldBeNull('document.createTouchList(new Array(5)).item(0)');
shouldBeNull('document.createTouchList("string").item(0)');
shouldBeNull('document.createTouchList(null).item(0)');
shouldBeNull('document.createTouchList(undefined).item(0)');

var t = document.createTouch(window, document.body, 12341, 60, 65, 100, 105);
var t2 = document.createTouch(window, document.body, 12342, 50, 55, 115, 120);
var tl = document.createTouchList(t, document, t2);

shouldBe('tl.length', '3');
shouldBeNonNull('tl.item(0)');
shouldBeNull('tl.item(1)');
shouldBeNonNull('tl.item(2)');

isSuccessfullyParsed();
