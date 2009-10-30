description('Test that the caret is positioned correctly when its offset occurrs in the middle of a ligature,\
    and that hit-testing in the middle of a ligature works correctly.');

var latin = document.createElement("div");
latin.innerHTML = "office";
latin.style.fontSize = "72px";
latin.style.textRendering = "optimizelegibility";
latin.style.position = "absolute";
latin.style.top = "0";
latin.style.left = "0";
document.body.appendChild(latin);

var y = latin.offsetTop + latin.offsetHeight / 2;

document.body.offsetTop;

shouldBe('document.caretRangeFromPoint(10, y).startOffset', '0');
shouldBe('document.caretRangeFromPoint(30, y).startOffset', '1');
shouldBe('document.caretRangeFromPoint(60, y).startOffset', '2');
shouldBe('document.caretRangeFromPoint(80, y).startOffset', '3');
shouldBe('document.caretRangeFromPoint(100, y).startOffset', '4');
shouldBe('document.caretRangeFromPoint(120, y).startOffset', '5');

var range = document.createRange();
range.setStart(latin.firstChild, 0);
range.setEnd(latin.firstChild, 3);
shouldBe('range.getBoundingClientRect().width', '80');

document.body.removeChild(latin);

var arabic = document.createElement("div");
arabic.innerHTML = "&#x062d;&#x0644;&#x0627;&#x062d;";
arabic.style.fontSize = "72px";
arabic.style.direction = "rtl";
arabic.style.position = "absolute";
arabic.style.top = "0";
arabic.style.right = "0";
document.body.appendChild(arabic);

y = arabic.offsetTop + arabic.offsetHeight / 2;
var x = arabic.offsetLeft + arabic.offsetWidth;

shouldBe('document.caretRangeFromPoint(x - 20, y).startOffset', '0');
shouldBe('document.caretRangeFromPoint(x - 50, y).startOffset', '1');
shouldBe('document.caretRangeFromPoint(x - 64, y).startOffset', '2');
shouldBe('document.caretRangeFromPoint(x - 90, y).startOffset', '3');

range.setStart(arabic.firstChild, 0);
range.setEnd(arabic.firstChild, 2);
var w = range.getBoundingClientRect().width;
// Widths vary between Mac OS X Leopard and current Mac OS X.
shouldBeTrue('w === 65 || w === 61');

document.body.removeChild(arabic);

var successfullyParsed = true;
