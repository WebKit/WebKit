description('Test parsing of the CSS wrap-margin property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitWrapMargin;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-wrap-margin", value);
    var webkitWrapMarginComputedValue = getComputedStyle(div).getPropertyValue("-webkit-wrap-margin");
    document.body.removeChild(div);
    return webkitWrapMarginComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-wrap-margin", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-wrap-margin", childValue);

    var childWebKitWrapMarginComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-wrap-margin");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitWrapMarginComputedValue;
}

shouldBeEqualToString('test("-webkit-wrap-margin: 0")', "0px");
shouldBeEqualToString('test("-webkit-wrap-margin: 1.5ex")', "1.5ex");
shouldBeEqualToString('test("-webkit-wrap-margin: 2em")', "2em");
shouldBeEqualToString('test("-webkit-wrap-margin: 2.5in")', "2.5in");
shouldBeEqualToString('test("-webkit-wrap-margin: 3cm")', "3cm");
shouldBeEqualToString('test("-webkit-wrap-margin: 3.5mm")', "3.5mm");
shouldBeEqualToString('test("-webkit-wrap-margin: 4pt")', "4pt");
shouldBeEqualToString('test("-webkit-wrap-margin: 4.5pc")', "4.5pc");
shouldBeEqualToString('test("-webkit-wrap-margin: 5px")', "5px");

shouldBeEqualToString('test("-webkit-wrap-margin: -5px")', "");
shouldBeEqualToString('test("-webkit-wrap-margin: auto")', "");
shouldBeEqualToString('test("-webkit-wrap-margin: \'string\'")', "");
shouldBeEqualToString('test("-webkit-wrap-margin: 120%")', "");

shouldBeEqualToString('testComputedStyle("0")', "0px");
shouldBeEqualToString('testComputedStyle("1px")', "1px");
shouldBeEqualToString('testComputedStyle("-5em")', "0px");
shouldBeEqualToString('testComputedStyle("identifier")', "0px");
shouldBeEqualToString('testComputedStyle("\'string\'")', "0px");

shouldBeEqualToString('testNotInherited("0", "0")', "0px");
shouldBeEqualToString('testNotInherited("0", "1px")', "1px");
shouldBeEqualToString('testNotInherited("1px", "-1em")', "0px");
shouldBeEqualToString('testNotInherited("2px", "1px")', "1px");
