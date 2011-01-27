description("Test path animation where coordinate modes of start and end differ. You should see PASS messages");
createSVGTestCase();
// FIXME: We should move to animatedPathSegList, once it is implemented.

// Setup test document
var path = createSVGElement("path");
path.setAttribute("id", "path");
path.setAttribute("d", "M -20 -20 C 20 -20 20 -20 20 20 S 20 40 -20 20 Z");
path.setAttribute("fill", "green");
path.setAttribute("onclick", "executeTest()");
path.setAttribute("transform", "translate(50, 50)");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "d");
animate.setAttribute("from", "M -20 -20 C 20 -20 20 -20 20 20 S 20 40 -20 20 Z");
animate.setAttribute("to", "M -20 -20 c 0 40 0 40 40 40 s 40 0 0 -40 z");
animate.setAttribute("begin", "click");
animate.setAttribute("dur", "4s");
path.appendChild(animate);
rootSVGElement.appendChild(path);

// Setup animation test
function sample1() {
    // Check initial/end conditions
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBe("path.pathSegList.getItem(0).x", "-20");
    shouldBe("path.pathSegList.getItem(0).y", "-20");
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'C'");
    shouldBe("path.pathSegList.getItem(1).x", "20");
    shouldBe("path.pathSegList.getItem(1).y", "20");
    shouldBe("path.pathSegList.getItem(1).x1", "20");
    shouldBe("path.pathSegList.getItem(1).y1", "-20");
    shouldBe("path.pathSegList.getItem(1).x2", "20");
    shouldBe("path.pathSegList.getItem(1).y2", "-20");
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'S'");
    shouldBe("path.pathSegList.getItem(2).x", "-20");
    shouldBe("path.pathSegList.getItem(2).y", "20");
    shouldBe("path.pathSegList.getItem(2).x2", "20");
    shouldBe("path.pathSegList.getItem(2).y2", "40");
}

function sample2() {
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "-20", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "-20", 0.01);
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'C'");
    shouldBeCloseEnough("path.pathSegList.getItem(1).x", "20", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y", "20", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x1", "10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y1", "-10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x2", "10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y2", "-10", 0.01);
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'S'");
    shouldBeCloseEnough("path.pathSegList.getItem(2).x", "-10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y", "10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).x2", "30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y2", "35", 0.01);
}

function sample3() {
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "-20", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "-20", 0.01);
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'c'");
    shouldBeCloseEnough("path.pathSegList.getItem(1).x", "40", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y", "40", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x1", "10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y1", "30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x2", "10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y2", "30", 0.01);
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'s'");
    shouldBeCloseEnough("path.pathSegList.getItem(2).x", "-10", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y", "-30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).x2", "30", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y2", "5", 0.01);
}

function sample4() {
    shouldBe("path.pathSegList.getItem(0).pathSegTypeAsLetter", "'M'");
    shouldBeCloseEnough("path.pathSegList.getItem(0).x", "-20", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(0).y", "-20", 0.01);
    shouldBe("path.pathSegList.getItem(1).pathSegTypeAsLetter", "'c'");
    shouldBeCloseEnough("path.pathSegList.getItem(1).x", "40", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y", "40", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x1", "0", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y1", "40", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).x2", "0", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(1).y2", "40", 0.01);
    shouldBe("path.pathSegList.getItem(2).pathSegTypeAsLetter", "'s'");
    shouldBeCloseEnough("path.pathSegList.getItem(2).x", "0", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y", "-40", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).x2", "40", 0.01);
    shouldBeCloseEnough("path.pathSegList.getItem(2).y2", "0", 0.01);
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, elementId, sampleCallback]
        ["animation", 0.0,    "path", sample1],
        ["animation", 1.0,    "path", sample2],
        ["animation", 3.0,    "path", sample3],
        ["animation", 3.9999, "path", sample4],
        ["animation", 4.0 ,   "path", sample1]
    ];

    runAnimationTest(expectedValues);
}

// Begin test async
window.setTimeout("triggerUpdate(50, 50)", 0);
var successfullyParsed = true;
