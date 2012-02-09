description('Test parsing of the CSS line-snap property.');

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitLineSnap;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-line-snap", value);
    var webkitFlowComputedValue = getComputedStyle(div).getPropertyValue("-webkit-line-snap");
    document.body.removeChild(div);
    return webkitFlowComputedValue;
}

function testInherited(parentValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-line-snap", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);

    var childWebKitFlowComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-line-snap");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitFlowComputedValue;
}

shouldBeEqualToString('test("-webkit-line-snap: none")', "none");
shouldBeEqualToString('test("-webkit-line-snap: baseline")', "baseline");
shouldBeEqualToString('test("-webkit-line-snap: contain")', "contain");
shouldBeEqualToString('test("-webkit-line-snap: ;")', "");
shouldBeEqualToString('test("-webkit-line-snap: 1")', "");
shouldBeEqualToString('test("-webkit-line-snap: 1.2")', "");
shouldBeEqualToString('test("-webkit-line-snap: -1")', "");
shouldBeEqualToString('test("-webkit-line-snap: 12px")', "");

shouldBeEqualToString('testComputedStyle("none")', "none");
shouldBeEqualToString('testComputedStyle("")', "none");
shouldBeEqualToString('testComputedStyle("12px")', "none");

shouldBeEqualToString('testInherited("none")', "none");
shouldBeEqualToString('testInherited("baseline")', "baseline");
shouldBeEqualToString('testInherited("contain")', "contain");
