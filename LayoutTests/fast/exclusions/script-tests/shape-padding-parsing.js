description('Test parsing of the CSS shape-padding property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitShapePadding;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-shape-padding", value);
    var webkitShapePaddingComputedValue = getComputedStyle(div).getPropertyValue("-webkit-shape-padding");
    document.body.removeChild(div);
    return webkitShapePaddingComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-shape-padding", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-shape-padding", childValue);

    var childWebKitShapePaddingComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-shape-padding");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitShapePaddingComputedValue;
}

shouldBeEqualToString('test("-webkit-shape-padding: 0")', "0px");
shouldBeEqualToString('test("-webkit-shape-padding: 1.5ex")', "1.5ex");
shouldBeEqualToString('test("-webkit-shape-padding: 2em")', "2em");
shouldBeEqualToString('test("-webkit-shape-padding: 2.5in")', "2.5in");
shouldBeEqualToString('test("-webkit-shape-padding: 3cm")', "3cm");
shouldBeEqualToString('test("-webkit-shape-padding: 3.5mm")', "3.5mm");
shouldBeEqualToString('test("-webkit-shape-padding: 4pt")', "4pt");
shouldBeEqualToString('test("-webkit-shape-padding: 4.5pc")', "4.5pc");
shouldBeEqualToString('test("-webkit-shape-padding: 5px")', "5px");

shouldBeEqualToString('test("-webkit-shape-padding: -5px")', "");
shouldBeEqualToString('test("-webkit-shape-padding: auto")', "");
shouldBeEqualToString('test("-webkit-shape-padding: \'string\'")', "");
shouldBeEqualToString('test("-webkit-shape-padding: 120%")', "");

shouldBeEqualToString('testComputedStyle("0")', "0px");
shouldBeEqualToString('testComputedStyle("1px")', "1px");
shouldBeEqualToString('testComputedStyle("-5em")', "0px");
shouldBeEqualToString('testComputedStyle("identifier")', "0px");
shouldBeEqualToString('testComputedStyle("\'string\'")', "0px");

shouldBeEqualToString('testNotInherited("0", "0")', "0px");
shouldBeEqualToString('testNotInherited("0", "1px")', "1px");
shouldBeEqualToString('testNotInherited("1px", "-1em")', "0px");
shouldBeEqualToString('testNotInherited("2px", "1px")', "1px");
