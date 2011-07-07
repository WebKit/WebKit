description('Test parsing of the CSS webkit-flow property.');

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitFlow;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-flow", value);
    var webkitFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-flow");
    document.body.removeChild(div);
    return webkitFlowComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-flow", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-flow", childValue);

    var childWebKitFlowComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-flow");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitFlowComputedValue;
}

shouldBeEqualToString('test("-webkit-flow: auto")', "auto");
shouldBeEqualToString('test("-webkit-flow: initial")', "initial");
shouldBeEqualToString('test("-webkit-flow: inherit")', "inherit");
shouldBeEqualToString('test("-webkit-flow: \'first-flow\'")', "first-flow");
shouldBeEqualToString('test("-webkit-flow: \'first flow\'")', "'first flow'");
shouldBeEqualToString('test("-webkit-flow: \'auto\';")', "auto");
shouldBeEqualToString('test("-webkit-flow: \'\'")', "auto");
shouldBeEqualToString('test("-webkit-flow: ;")', "");
shouldBeEqualToString('test("-webkit-flow: 1")', "");
shouldBeEqualToString('test("-webkit-flow: 1.2")', "");
shouldBeEqualToString('test("-webkit-flow: -1")', "");
shouldBeEqualToString('test("-webkit-flow: 12px")', "");
shouldBeEqualToString('test("-webkit-flow: first-flow;")', "");
shouldBeEqualToString('test("-webkit-flow: first flow")', "");

shouldBeEqualToString('testComputedStyle("auto")', "auto");
shouldBeEqualToString('testComputedStyle("")', "auto");
shouldBeEqualToString('testComputedStyle("\'auto\'")', "auto");
shouldBeEqualToString('testComputedStyle("\'first-flow\'")', "first-flow");
shouldBeEqualToString('testComputedStyle("first-flow")', "auto");
shouldBeEqualToString('testComputedStyle("inherited")', "auto");
shouldBeEqualToString('testComputedStyle("initial")', "auto");
shouldBeEqualToString('testComputedStyle("12px")', "auto");

shouldBeEqualToString('testNotInherited("auto", "auto")', "auto");
shouldBeEqualToString('testNotInherited("auto", "\'child-flow\'")', "child-flow");
shouldBeEqualToString('testNotInherited("\'parent-flow\'", "auto")', "auto");
shouldBeEqualToString('testNotInherited("\'parent-flow\'", "\'child-flow\'")', "child-flow");

successfullyParsed = true;
