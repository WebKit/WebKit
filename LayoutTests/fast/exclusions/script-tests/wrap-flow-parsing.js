description('Test parsing of the CSS wrap-flow property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitWrapFlow;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-wrap-flow", value);
    var webkitWrapFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-wrap-flow");
    document.body.removeChild(div);
    return webkitWrapFlowComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-wrap-flow", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-wrap-flow", childValue);

    var childWebKitWrapFlowComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-wrap-flow");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitWrapFlowComputedValue;
}

shouldBeEqualToString('test("-webkit-wrap-flow: auto")', "auto");
shouldBeEqualToString('test("-webkit-wrap-flow: both")', "both");
shouldBeEqualToString('test("-webkit-wrap-flow: start")', "start");
shouldBeEqualToString('test("-webkit-wrap-flow: end")', "end");
shouldBeEqualToString('test("-webkit-wrap-flow: maximum")', "maximum");
shouldBeEqualToString('test("-webkit-wrap-flow: clear")', "clear");

shouldBeEqualToString('test("-webkit-wrap-flow: ;")', "");
shouldBeEqualToString('test("-webkit-wrap-flow: 5")', "");
shouldBeEqualToString('test("-webkit-wrap-flow: -1.2")', "");
shouldBeEqualToString('test("-webkit-wrap-flow: \'string\'")', "");

shouldBeEqualToString('testComputedStyle("auto")', "auto");
shouldBeEqualToString('testComputedStyle("5")', "auto");
shouldBeEqualToString('testComputedStyle("\'string\'")', "auto");

shouldBeEqualToString('testNotInherited("auto", "start")', "start");
shouldBeEqualToString('testNotInherited("end", "auto")', "auto");
shouldBeEqualToString('testNotInherited("both", "clear")', "clear");
