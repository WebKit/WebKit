description("Test fallback to 'to' animation if user specifies 'by' animation on path. You should see a green 100x100 path and only PASS messages");
createSVGTestCase();
// FIXME: We should move to animatePathSegList, once it is implemented.

// Setup test document
var path = createSVGElement("path");
path.setAttribute("id", "path");
path.setAttribute("d", "M 40 40 L 60 40 L 60 60 L 40 60 z");
path.setAttribute("fill", "green");
path.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "d");
animate.setAttribute("from", "M 40 40 L 60 40 L 60 60 L 40 60 z");
animate.setAttribute("by", "M 0 0 L 100 0 L 100 100 L 0 100 z");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
path.appendChild(animate);
rootSVGElement.appendChild(path);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "40");
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "40");
}

function sample2() {
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "20");
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "20");
}

function sample3() {
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "0");
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "0");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 2.0,   sample2],
        ["animation", 3.999, sample3],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
