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

var successfullyParsed = true;

