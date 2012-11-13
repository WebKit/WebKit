description('Test parsing of the CSS shape-margin property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitShapeMargin;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-shape-margin", value);
    var webkitShapeMarginComputedValue = getComputedStyle(div).getPropertyValue("-webkit-shape-margin");
    document.body.removeChild(div);
    return webkitShapeMarginComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-shape-margin", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-shape-margin", childValue);

    var childWebKitShapeMarginComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-shape-margin");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitShapeMarginComputedValue;
}

shouldBeEqualToString('test("-webkit-shape-margin: 0")', "0px");
shouldBeEqualToString('test("-webkit-shape-margin: 1.5ex")', "1.5ex");
shouldBeEqualToString('test("-webkit-shape-margin: 2em")', "2em");
shouldBeEqualToString('test("-webkit-shape-margin: 2.5in")', "2.5in");
shouldBeEqualToString('test("-webkit-shape-margin: 3cm")', "3cm");
shouldBeEqualToString('test("-webkit-shape-margin: 3.5mm")', "3.5mm");
shouldBeEqualToString('test("-webkit-shape-margin: 4pt")', "4pt");
shouldBeEqualToString('test("-webkit-shape-margin: 4.5pc")', "4.5pc");
shouldBeEqualToString('test("-webkit-shape-margin: 5px")', "5px");

shouldBeEqualToString('test("-webkit-shape-margin: -5px")', "");
shouldBeEqualToString('test("-webkit-shape-margin: auto")', "");
shouldBeEqualToString('test("-webkit-shape-margin: \'string\'")', "");
shouldBeEqualToString('test("-webkit-shape-margin: 120%")', "");

shouldBeEqualToString('testComputedStyle("0")', "0px");
shouldBeEqualToString('testComputedStyle("1px")', "1px");
shouldBeEqualToString('testComputedStyle("-5em")', "0px");
shouldBeEqualToString('testComputedStyle("identifier")', "0px");
shouldBeEqualToString('testComputedStyle("\'string\'")', "0px");

shouldBeEqualToString('testNotInherited("0", "0")', "0px");
shouldBeEqualToString('testNotInherited("0", "1px")', "1px");
shouldBeEqualToString('testNotInherited("1px", "-1em")', "0px");
shouldBeEqualToString('testNotInherited("2px", "1px")', "1px");
