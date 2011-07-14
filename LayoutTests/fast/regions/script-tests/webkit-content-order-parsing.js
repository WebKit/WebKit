description('Tests parsing of webkit-content-order property');

var webkitContentOrderProperty = "-webkit-content-order";

function testCSSText(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitContentOrder;
}

function testComputedStyle(declaration) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty(webkitContentOrderProperty, declaration);

    var contentComputedValue = getComputedStyle(div).getPropertyValue(webkitContentOrderProperty);
    document.body.removeChild(div);
    return contentComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty(webkitContentOrderProperty, parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty(webkitContentOrderProperty, childValue);

    var childWebKitFlowComputedValue = getComputedStyle(childDiv).getPropertyValue(webkitContentOrderProperty);

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitFlowComputedValue;
}
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': auto")', "");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': initial")', "initial");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': inherit")', "inherit");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': 1")', "1");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': 0")', "0");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': -1")', "-1");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': 2147483647")', "2147483647");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': 2147483648")', "2147483648");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': -2147483648")', "-2147483648");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': -2147483649")', "-2147483649");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': 12.5")', "");
shouldBeEqualToString('testCSSText("' + webkitContentOrderProperty + ': 1px")', "");

shouldBeEqualToString('testComputedStyle("auto")', "0");
shouldBeEqualToString('testComputedStyle("initial")', "0");
shouldBeEqualToString('testComputedStyle("inherit")', "0");
shouldBeEqualToString('testComputedStyle("1")', "1");
shouldBeEqualToString('testComputedStyle("0")', "0");
shouldBeEqualToString('testComputedStyle("-1")', "-1");
shouldBeEqualToString('testComputedStyle("2147483647")', "2147483647");
shouldBeEqualToString('testComputedStyle("2147483648")', "2147483647");
shouldBeEqualToString('testComputedStyle("-2147483648")', "-2147483648");
shouldBeEqualToString('testComputedStyle("-2147483649")', "-2147483648");
shouldBeEqualToString('testComputedStyle("12.5")', "0");
shouldBeEqualToString('testComputedStyle("1px")', "0");

shouldBeEqualToString('testNotInherited("0", "0")', "0");
shouldBeEqualToString('testNotInherited("0", "1")', "1");
shouldBeEqualToString('testNotInherited("1", "0")', "0");
shouldBeEqualToString('testNotInherited("1", "2")', "2");

successfullyParsed = true;
