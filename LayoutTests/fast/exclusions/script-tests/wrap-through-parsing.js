description('Test parsing of the CSS wrap-through property.');

if (window.internals)
    window.internals.settings.setCSSExclusionsEnabled(true);

function test(declaration) {
    var div = document.createElement("div");
    div.setAttribute("style", declaration);
    return div.style.webkitWrapThrough;
}

function testComputedStyle(value) {
    var div = document.createElement("div");
    document.body.appendChild(div);
    div.style.setProperty("-webkit-wrap-through", value);
    var webkitWrapThroughComputedValue = getComputedStyle(div).getPropertyValue("-webkit-wrap-through");
    document.body.removeChild(div);
    return webkitWrapThroughComputedValue;
}

function testNotInherited(parentValue, childValue) {
    var parentDiv = document.createElement("div");
    document.body.appendChild(parentDiv);
    parentDiv.style.setProperty("-webkit-wrap-through", parentValue);

    var childDiv = document.createElement("div");
    parentDiv.appendChild(childDiv);
    childDiv.style.setProperty("-webkit-wrap-through", childValue);

    var childWebKitWrapThroughComputedValue = getComputedStyle(childDiv).getPropertyValue("-webkit-wrap-through");

    parentDiv.removeChild(childDiv);
    document.body.removeChild(parentDiv);

    return childWebKitWrapThroughComputedValue;
}

shouldBeEqualToString('test("-webkit-wrap-through: wrap")', "wrap");
shouldBeEqualToString('test("-webkit-wrap-through: none")', "none");

shouldBeEqualToString('test("-webkit-wrap-through: ;")', "");
shouldBeEqualToString('test("-webkit-wrap-through: 5")', "");
shouldBeEqualToString('test("-webkit-wrap-through: -1.2")', "");
shouldBeEqualToString('test("-webkit-wrap-through: \'string\'")', "");

shouldBeEqualToString('testComputedStyle("wrap")', "wrap");
shouldBeEqualToString('testComputedStyle("5")', "wrap");
shouldBeEqualToString('testComputedStyle("\'string\'")', "wrap");

shouldBeEqualToString('testNotInherited("wrap", "none")', "none");
shouldBeEqualToString('testNotInherited("none", "wrap")', "wrap");
