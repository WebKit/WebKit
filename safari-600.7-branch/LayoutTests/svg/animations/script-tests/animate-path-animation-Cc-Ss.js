description("Test path animation where coordinate modes of start and end differ. You should see PASS messages");
createSVGTestCase();

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
function checkBaseVal() {
    shouldBe("path.pathSegList.numberOfItems", "4");
    shouldBeEqualToString("path.pathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBe("path.pathSegList.getItem(0).x", "-20");
    shouldBe("path.pathSegList.getItem(0).y", "-20");
    shouldBeEqualToString("path.pathSegList.getItem(1).pathSegTypeAsLetter", "C");
    shouldBe("path.pathSegList.getItem(1).x", "20");
    shouldBe("path.pathSegList.getItem(1).y", "20");
    shouldBe("path.pathSegList.getItem(1).x1", "20");
    shouldBe("path.pathSegList.getItem(1).y1", "-20");
    shouldBe("path.pathSegList.getItem(1).x2", "20");
    shouldBe("path.pathSegList.getItem(1).y2", "-20");
    shouldBeEqualToString("path.pathSegList.getItem(2).pathSegTypeAsLetter", "S");
    shouldBe("path.pathSegList.getItem(2).x", "-20");
    shouldBe("path.pathSegList.getItem(2).y", "20");
    shouldBe("path.pathSegList.getItem(2).x2", "20");
    shouldBe("path.pathSegList.getItem(2).y2", "40");
    shouldBeEqualToString("path.pathSegList.getItem(3).pathSegTypeAsLetter", "Z");
}

function sample1() {
    // Check initial/end conditions
    shouldBe("path.animatedPathSegList.numberOfItems", "4");
    shouldBeEqualToString("path.animatedPathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).x", "-20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).y", "-20");
    shouldBeEqualToString("path.animatedPathSegList.getItem(1).pathSegTypeAsLetter", "C");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x1", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y1", "-20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x2", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y2", "-20");
    shouldBeEqualToString("path.animatedPathSegList.getItem(2).pathSegTypeAsLetter", "S");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x", "-20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x2", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y2", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(3).pathSegTypeAsLetter", "Z");
    checkBaseVal();
}

function sample2() {
    shouldBe("path.animatedPathSegList.numberOfItems", "4");
    shouldBeEqualToString("path.animatedPathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).x", "-20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).y", "-20");
    shouldBeEqualToString("path.animatedPathSegList.getItem(1).pathSegTypeAsLetter", "C");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y", "20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x1", "10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y1", "-10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x2", "10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y2", "-10");
    shouldBeEqualToString("path.animatedPathSegList.getItem(2).pathSegTypeAsLetter", "S");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x", "-10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y", "10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x2", "30");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y2", "35");
    shouldBeEqualToString("path.animatedPathSegList.getItem(3).pathSegTypeAsLetter", "Z");
    checkBaseVal();
}

function sample3() {
    shouldBe("path.animatedPathSegList.numberOfItems", "4");
    shouldBeEqualToString("path.animatedPathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).x", "-20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).y", "-20");
    shouldBeEqualToString("path.animatedPathSegList.getItem(1).pathSegTypeAsLetter", "c");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x1", "10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y1", "30");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x2", "10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y2", "30");
    shouldBeEqualToString("path.animatedPathSegList.getItem(2).pathSegTypeAsLetter", "s");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x", "-10");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y", "-30");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x2", "30");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y2", "5");
    shouldBeEqualToString("path.animatedPathSegList.getItem(3).pathSegTypeAsLetter", "Z");
    checkBaseVal();
}

function sample4() {
    shouldBe("path.animatedPathSegList.numberOfItems", "4");
    shouldBeEqualToString("path.animatedPathSegList.getItem(0).pathSegTypeAsLetter", "M");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).x", "-20");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(0).y", "-20");
    shouldBeEqualToString("path.animatedPathSegList.getItem(1).pathSegTypeAsLetter", "c");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x1", "0");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y1", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).x2", "0");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(1).y2", "40");
    shouldBeEqualToString("path.animatedPathSegList.getItem(2).pathSegTypeAsLetter", "s");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x", "-0");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y", "-40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).x2", "40");
    shouldBeCloseEnough("path.animatedPathSegList.getItem(2).y2", "0");
    shouldBeEqualToString("path.animatedPathSegList.getItem(3).pathSegTypeAsLetter", "Z");
    checkBaseVal();
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.0,   sample2],
        ["animation", 3.0,   sample3],
        ["animation", 3.999, sample4],
        ["animation", 4.001, sample1]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
