description('Test parsing of the CSS line-grid property.');

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitLineGrid;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-line-grid", value);
    var webkitFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-line-grid");
    document.body.removeChild(div);
    return webkitFlowComputedValue;
}

function testInherited(parentValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-line-grid", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);

    var childWebKitFlowComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-line-grid");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitFlowComputedValue;
}

shouldBeEqualToString('test("-webkit-line-grid: none")', "none");
shouldBeEqualToString('test("-webkit-line-grid: first-grid")', "first-grid");
shouldBeEqualToString('test("-webkit-line-grid: \'first grid\'")', "");
shouldBeEqualToString('test("-webkit-line-grid: ;")', "");
shouldBeEqualToString('test("-webkit-line-grid: 1")', "");
shouldBeEqualToString('test("-webkit-line-grid: 1.2")', "");
shouldBeEqualToString('test("-webkit-line-grid: -1")', "");
shouldBeEqualToString('test("-webkit-line-grid: 12px")', "");

shouldBeEqualToString('testComputedStyle("none")', "none");
shouldBeEqualToString('testComputedStyle("")', "none");
shouldBeEqualToString('testComputedStyle("\'first-flow\'")', "none");
shouldBeEqualToString('testComputedStyle("first-grid")', "first-grid");
shouldBeEqualToString('testComputedStyle("12px")', "none");

shouldBeEqualToString('testInherited("none")', "none");
shouldBeEqualToString('testInherited("grid")', "grid");
