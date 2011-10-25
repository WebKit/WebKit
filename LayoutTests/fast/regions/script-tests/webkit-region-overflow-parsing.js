description('Tests parsing of webkit-region-overflow property');

var webkitRegionOverflowProperty = "-webkit-region-overflow";

function testCSSText(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitRegionOverflow;
}

function testComputedStyle(declaration) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty(webkitRegionOverflowProperty, declaration);

    var contentComputedValue = getComputedStyle(div).getPropertyValue(webkitRegionOverflowProperty);
    document.body.removeChild(div);
    return contentComputedValue;
}

shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': auto")', "auto");
shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': initial")', "initial");
shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': inherit")', "inherit");
shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': break")', "break");
shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': 0")', "");
shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': -1")', "");
shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': 12.5")', "");
shouldBeEqualToString('testCSSText("' + webkitRegionOverflowProperty + ': 1px")', "");

shouldBeEqualToString('testComputedStyle("auto")', "auto");
shouldBeEqualToString('testComputedStyle("initial")', "auto");
shouldBeEqualToString('testComputedStyle("inherit")', "auto");
shouldBeEqualToString('testComputedStyle("break")', "break");
shouldBeEqualToString('testComputedStyle("0")', "auto");
shouldBeEqualToString('testComputedStyle("-1")', "auto");
shouldBeEqualToString('testComputedStyle("12.5")', "auto");
shouldBeEqualToString('testComputedStyle("1px")', "auto");
