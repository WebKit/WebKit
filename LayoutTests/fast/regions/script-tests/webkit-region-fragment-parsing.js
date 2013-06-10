description('Tests parsing of webkit-region-fragment property');

var webkitRegionFragmentProperty = "-webkit-region-fragment";

function testCSSText(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitRegionFragment;
}

function testComputedStyle(declaration) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty(webkitRegionFragmentProperty, declaration);

    var contentComputedValue = getComputedStyle(div).getPropertyValue(webkitRegionFragmentProperty);
    document.body.removeChild(div);
    return contentComputedValue;
}

shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': auto")', "auto");
shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': initial")', "initial");
shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': inherit")', "inherit");
shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': break")', "break");
shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': 0")', "");
shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': -1")', "");
shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': 12.5")', "");
shouldBeEqualToString('testCSSText("' + webkitRegionFragmentProperty + ': 1px")', "");

shouldBeEqualToString('testComputedStyle("auto")', "auto");
shouldBeEqualToString('testComputedStyle("initial")', "auto");
shouldBeEqualToString('testComputedStyle("inherit")', "auto");
shouldBeEqualToString('testComputedStyle("break")', "break");
shouldBeEqualToString('testComputedStyle("0")', "auto");
shouldBeEqualToString('testComputedStyle("-1")', "auto");
shouldBeEqualToString('testComputedStyle("12.5")', "auto");
shouldBeEqualToString('testComputedStyle("1px")', "auto");
