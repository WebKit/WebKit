
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
test(function() { assert_equals(gebi('default').clientHeight, referenceHorizontalHeight); }, "default height");
test(function() { assert_equals(gebi('horizontal').clientHeight, referenceHorizontalHeight); }, "horizontal height");
test(function() { assert_equals(gebi('vertical').clientHeight, referenceVerticalHeight); }, "vertical height");
test(function() { assert_equals(gebi('toHorizontal').clientHeight, referenceHorizontalHeight); }, "toHorizontal height");
test(function() { assert_equals(gebi('toVertical').clientHeight, referenceVerticalHeight); }, "toVertical height");
