description("Test the behavior of currentPath in Canvas");
var ctx = document.createElement('canvas').getContext('2d');

var testStrings = [
    "ctx.isPointInPath(49,49)",
    "ctx.isPointInPath(99,99)",
    "ctx.isPointInPath(149,149)",
    "ctx.isPointInPath(199,199)",
    "ctx.isPointInPath(249,249)"
];

// Test collection of points. Each point has an offset of 50,50 to previous point.
function testPointCollection(hitResults) {
    for (var i = 0; i < hitResults.length; i++) {
        if (hitResults[i])
            shouldBeTrue(testStrings[i]);
        else
            shouldBeFalse(testStrings[i]);
    }
}

document.body.appendChild(ctx.canvas);

ctx.fillStyle = '#0f0';
ctx.beginPath();

debug("Create path object, replace current context path with the path of this object.");
var p = new Path();
p.rect(0,0,200,200);
testPointCollection([false, false, false, false, false]);

ctx.currentPath = p;

testPointCollection([true, true, true, true, false]);
debug("");

debug("Add new segment to context path and check that this is not added to the path object (not live).")

ctx.rect(50,50,200,200);
testPointCollection([true, true, true, true, true]);

ctx.currentPath = p;

testPointCollection([true, true, true, true, false]);
debug("");

debug("Test that path object can get applied to transformed context, respecting the CTM.");

ctx.beginPath();
ctx.translate(100,100);
ctx.currentPath = p;
ctx.translate(-100,-100);
testPointCollection([false, false, true, true, true]);

debug("");

debug("Test that currentPath returns a path object.");
p = null;
shouldBeNull("p");
ctx.beginPath();
ctx.rect(0,0,200,200);
p = ctx.currentPath;
shouldBeType("p", "Path");
debug("");

debug("Create context path and test that it exists.");
testPointCollection([true, true, true, true, false]);
debug("");

debug("Clear context path.");
ctx.beginPath();
testPointCollection([false, false, false, false, false]);
debug("");

debug("Apply stored (non-live) path object back to context.");
ctx.currentPath = p;
testPointCollection([true, true, true, true, false]);
debug("");

debug("Transform CTM in the process of adding segments to context path. Check that currentPath's path object archive these transformations.");
ctx.beginPath();
ctx.rect(0,0,100,100);
ctx.translate(150,150);
ctx.rect(0,0,100,100);
ctx.translate(-150,-150);
testPointCollection([true, true, false, true, true]);
p = ctx.currentPath;

debug("Clear current path on object and check that it is cleaned up.");
ctx.beginPath();
testPointCollection([false, false, false, false, false]);
debug("");

debug("Apply path back to context path.")
ctx.currentPath = p;
testPointCollection([true, true, false, true, true]);
