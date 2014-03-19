description("Series of tests to ensure correct results of isPointInPath with Path2D argument.");

var ctx = document.createElement('canvas').getContext('2d');
document.body.appendChild(ctx.canvas);

var path = new Path2D();
path.rect(0, 0, 100, 100);
path.rect(25, 25, 50, 50);
shouldBeTrue("ctx.isPointInPath(path, 50, 50)");
shouldBeFalse("ctx.isPointInPath(path, NaN, 50)");
shouldBeFalse("ctx.isPointInPath(path, 50, NaN)");

path = new Path2D();
path.rect(0, 0, 100, 100);
path.rect(25, 25, 50, 50);
shouldBeTrue("ctx.isPointInPath(path, 50, 50, 'nonzero')");

path = new Path2D();
path.rect(0, 0, 100, 100);
path.rect(25, 25, 50, 50);
shouldBeFalse("ctx.isPointInPath(path, 50, 50, 'evenodd')");

ctx.translate(100,100);
shouldBeFalse("ctx.isPointInPath(path, 50, 50, 'nonzero')");

shouldThrow("ctx.isPointInPath(null, 50, 50)");
shouldThrow("ctx.isPointInPath(null, 50, 50, 'nonzero')");
shouldThrow("ctx.isPointInPath(null, 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath([], 50, 50)");
shouldThrow("ctx.isPointInPath([], 50, 50, 'nonzero')");
shouldThrow("ctx.isPointInPath([], 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath({}, 50, 50)");
shouldThrow("ctx.isPointInPath({}, 50, 50, 'nonzero')");
shouldThrow("ctx.isPointInPath({}, 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath('path2d', 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath(undefined, 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath(Number.MAX_VALUE, 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath(function() {}, 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath(false, 50, 50, 'evenodd')");
shouldThrow("ctx.isPointInPath(new Date(), 50, 50, 'evenodd')");