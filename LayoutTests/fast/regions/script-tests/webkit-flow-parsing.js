description('Test parsing of the CSS webkit-flow-into property.');

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitFlowInto;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-flow-into", value);
    var webkitFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-flow-into");
    document.body.removeChild(div);
    return webkitFlowComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-flow-into", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-flow-into", childValue);

    var childWebKitFlowComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-flow-into");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitFlowComputedValue;
}

shouldBeEqualToString('test("-webkit-flow-into: auto")', "auto");
shouldBeEqualToString('test("-webkit-flow-into: first-flow")', "first-flow");
shouldBeEqualToString('test("-webkit-flow-into: \'first flow\'")', "");
shouldBeEqualToString('test("-webkit-flow-into: ;")', "");
shouldBeEqualToString('test("-webkit-flow-into: 1")', "");
shouldBeEqualToString('test("-webkit-flow-into: 1.2")', "");
shouldBeEqualToString('test("-webkit-flow-into: -1")', "");
shouldBeEqualToString('test("-webkit-flow-into: 12px")', "");

shouldBeEqualToString('testComputedStyle("auto")', "auto");
shouldBeEqualToString('testComputedStyle("")', "auto");
shouldBeEqualToString('testComputedStyle("\'first-flow\'")', "auto");
shouldBeEqualToString('testComputedStyle("first-flow")', "first-flow");
shouldBeEqualToString('testComputedStyle("12px")', "auto");

shouldBeEqualToString('testNotInherited("auto", "auto")', "auto");
shouldBeEqualToString('testNotInherited("auto", "child-flow")', "child-flow");
shouldBeEqualToString('testNotInherited("parent-flow", "auto")', "auto");
shouldBeEqualToString('testNotInherited("parent-flow", "child-flow")', "child-flow");

successfullyParsed = true;
