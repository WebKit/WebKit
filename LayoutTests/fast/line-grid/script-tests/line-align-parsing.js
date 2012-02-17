description('Test parsing of the CSS line-snap property.');

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitLineAlign;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-line-align", value);
    var webkitFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-line-align");
    document.body.removeChild(div);
    return webkitFlowComputedValue;
}

function testInherited(parentValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-line-align", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);

    var childWebKitFlowComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-line-align");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitFlowComputedValue;
}

shouldBeEqualToString('test("-webkit-line-align: none")', "none");
shouldBeEqualToString('test("-webkit-line-align: edges")', "edges");
shouldBeEqualToString('test("-webkit-line-align: ;")', "");
shouldBeEqualToString('test("-webkit-line-align: 1")', "");
shouldBeEqualToString('test("-webkit-line-align: 1.2")', "");
shouldBeEqualToString('test("-webkit-line-align: -1")', "");
shouldBeEqualToString('test("-webkit-line-align: 12px")', "");

shouldBeEqualToString('testComputedStyle("none")', "none");
shouldBeEqualToString('testComputedStyle("")', "none");
shouldBeEqualToString('testComputedStyle("12px")', "none");

shouldBeEqualToString('testInherited("none")', "none");
shouldBeEqualToString('testInherited("edges")', "edges");

