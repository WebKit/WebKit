description("Check if box-orient is working in &lt;button&gt;. See https://bugs.webkit.org/show_bug.cgi?id=25406");

function setBoxOrient(element, boxOrient) {
    element.style.webkitBoxOrient = boxOrient;
    element.style.mozBoxOrient = boxOrient;
}

function gebi(id) {
    return document.getElementById(id);
}

setBoxOrient(gebi("toVertical"), "vertical");
setBoxOrient(gebi("toHorizontal"), "horizontal");

var referenceHorizontalHeight = gebi("reference_horizontal").clientHeight;
var referenceVerticalHeight = gebi("reference_vertical").clientHeight;
shouldBe("gebi('default').clientHeight", "referenceHorizontalHeight");
shouldBe("gebi('horizontal').clientHeight", "referenceHorizontalHeight");
shouldBe("gebi('vertical').clientHeight", "referenceVerticalHeight");
shouldBe("gebi('toHorizontal').clientHeight", "referenceHorizontalHeight");
shouldBe("gebi('toVertical').clientHeight", "referenceVerticalHeight");

// If we are in DTR, we don't need meaningless messages.
if (window.testRunner)
    document.getElementById("main").innerHTML = "";
