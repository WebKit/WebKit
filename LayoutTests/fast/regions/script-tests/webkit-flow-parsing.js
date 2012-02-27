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

shouldBeEqualToString('test("-webkit-flow-into: none")', "none");
shouldBeEqualToString('test("-webkit-flow-into: first-flow")', "first-flow");
shouldBeEqualToString('test("-webkit-flow-into: \'first flow\'")', "");
shouldBeEqualToString('test("-webkit-flow-into: ;")', "");
shouldBeEqualToString('test("-webkit-flow-into: 1")', "");
shouldBeEqualToString('test("-webkit-flow-into: 1.2")', "");
shouldBeEqualToString('test("-webkit-flow-into: -1")', "");
shouldBeEqualToString('test("-webkit-flow-into: 12px")', "");

shouldBeEqualToString('testComputedStyle("none")', "none");
shouldBeEqualToString('testComputedStyle("")', "none");
shouldBeEqualToString('testComputedStyle("\'first-flow\'")', "none");
shouldBeEqualToString('testComputedStyle("first-flow")', "first-flow");
shouldBeEqualToString('testComputedStyle("12px")', "none");

shouldBeEqualToString('testNotInherited("none", "none")', "none");
shouldBeEqualToString('testNotInherited("none", "child-flow")', "child-flow");
shouldBeEqualToString('testNotInherited("parent-flow", "none")', "none");
shouldBeEqualToString('testNotInherited("parent-flow", "child-flow")', "child-flow");
