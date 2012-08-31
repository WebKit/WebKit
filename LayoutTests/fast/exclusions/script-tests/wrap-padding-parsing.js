description('Test parsing of the CSS wrap-padding property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitWrapPadding;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-wrap-padding", value);
    var webkitWrapPaddingComputedValue = getComputedStyle(div).getPropertyValue("-webkit-wrap-padding");
    document.body.removeChild(div);
    return webkitWrapPaddingComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-wrap-padding", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-wrap-padding", childValue);

    var childWebKitWrapPaddingComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-wrap-padding");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitWrapPaddingComputedValue;
}

shouldBeEqualToString('test("-webkit-wrap-padding: 0")', "0px");
shouldBeEqualToString('test("-webkit-wrap-padding: 1.5ex")', "1.5ex");
shouldBeEqualToString('test("-webkit-wrap-padding: 2em")', "2em");
shouldBeEqualToString('test("-webkit-wrap-padding: 2.5in")', "2.5in");
shouldBeEqualToString('test("-webkit-wrap-padding: 3cm")', "3cm");
shouldBeEqualToString('test("-webkit-wrap-padding: 3.5mm")', "3.5mm");
shouldBeEqualToString('test("-webkit-wrap-padding: 4pt")', "4pt");
shouldBeEqualToString('test("-webkit-wrap-padding: 4.5pc")', "4.5pc");
shouldBeEqualToString('test("-webkit-wrap-padding: 5px")', "5px");

shouldBeEqualToString('test("-webkit-wrap-padding: -5px")', "");
shouldBeEqualToString('test("-webkit-wrap-padding: auto")', "");
shouldBeEqualToString('test("-webkit-wrap-padding: \'string\'")', "");
shouldBeEqualToString('test("-webkit-wrap-padding: 120%")', "");

shouldBeEqualToString('testComputedStyle("0")', "0px");
shouldBeEqualToString('testComputedStyle("1px")', "1px");
shouldBeEqualToString('testComputedStyle("-5em")', "0px");
shouldBeEqualToString('testComputedStyle("identifier")', "0px");
shouldBeEqualToString('testComputedStyle("\'string\'")', "0px");

shouldBeEqualToString('testNotInherited("0", "0")', "0px");
shouldBeEqualToString('testNotInherited("0", "1px")', "1px");
shouldBeEqualToString('testNotInherited("1px", "-1em")', "0px");
shouldBeEqualToString('testNotInherited("2px", "1px")', "1px");
