description("Check if box-orient is working in &lt;button&gt;. See https://bugs.webkit.org/show_bug.cgi?id=25406");

function setFlexDirection(element, flexDirection) {
    element.style.webkitFlexDirection = flexDirection;
    element.style.flexDirection = flexDirection;
}

function gebi(id) {
    return document.getElementById(id);
}

setFlexDirection(gebi("toVertical"), "column");
setFlexDirection(gebi("toHorizontal"), "row");

var referenceHorizontalHeight = gebi("reference_horizontal").clientHeight;
var referenceVerticalHeight = gebi("reference_vertical").clientHeight;
shouldBe("gebi('default').clientHeight", "referenceHorizontalHeight");
shouldBe("gebi('horizontal').clientHeight", "referenceHorizontalHeight");
shouldBe("gebi('vertical').clientHeight", "referenceVerticalHeight");
shouldBe("gebi('toHorizontal').clientHeight", "referenceHorizontalHeight");
shouldBe("gebi('toVertical').clientHeight", "referenceVerticalHeight");

// If we are in DRT, we don't need meaningless messages.
if (window.testRunner)
    document.getElementById("main").innerHTML = "";
