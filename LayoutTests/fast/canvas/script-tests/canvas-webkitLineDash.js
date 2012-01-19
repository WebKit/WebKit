description("Basic test for webkitLineDash and webkitLineDashOffset");

var canvas = document.createElement('canvas');
document.body.appendChild(canvas);
canvas.setAttribute('width', '700');
canvas.setAttribute('height', '700');
var ctx = canvas.getContext('2d');

// Verify default values.
shouldBe('ctx.webkitLineDashOffset', '0');

// Set dash-style.
ctx.webkitLineDash = [15, 10];
ctx.webkitLineDashOffset = 5;
ctx.strokeRect (10,10,100,100);

// Verify dash and offset.
var lineDash;
lineDash = ctx.webkitLineDash;
shouldBe('lineDash[0]', '15');
shouldBe('lineDash[1]', '10');
shouldBe('ctx.webkitLineDashOffset', '5');

// Verify that line dash offset persists after
// clearRect (which causes a save/restore of the context
// state to the stack).
ctx.clearRect(0, 0, 700, 700);
shouldBe('ctx.webkitLineDashOffset', '5');
