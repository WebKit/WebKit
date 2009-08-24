description("This tests that the SVGAnimatedNumberList returned by SVGTextPositioningElement.rotate is live.");

var textElement = document.createElementNS("http://www.w3.org/2000/svg", "text");
textElement.setAttribute("rotate", "10 20 30");

var rotate = textElement.rotate.baseVal;

function getRotate() {
    var i, a = [];
    for (i = 0; i < rotate.numberOfItems; i++) {
        a.push(rotate.getItem(i).value);
    }
    return String(a);
}

function getAndSetRotate(s) {
    textElement.setAttribute("rotate", s);
    return getRotate();
}

shouldBe("getRotate()", "'10,20,30'");
shouldBe("getAndSetRotate('40 50')", "'40,50'");
shouldBe("getAndSetRotate('6e1')", "'60'");
shouldBe("getAndSetRotate('-.5  , 40E-0')", "'-0.5,40'");

successfullyParsed = true;
