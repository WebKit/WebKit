function getCSSText(property, value)
{
    var element = document.createElement("div");
    element.style.cssText = property + ": " + value;
    return element.style[property];
}

function getComputedStyleValue(property, value) 
{
    var element = document.createElement("div");
    document.body.appendChild(element);
    element.style.setProperty(property, value);
    var computedValue = getComputedStyle(element).getPropertyValue(property);
    document.body.removeChild(element);
    return computedValue;
}

function getParentAndChildComputedStyles(property, parentValue, childValue) 
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style.setProperty(property, parentValue);
    var childElement = document.createElement("div");
    parentElement.appendChild(childElement);
    childElement.style.setProperty(property, childValue);
    var parentComputedValue = getComputedStyle(parentElement).getPropertyValue(property);
    var childComputedValue = getComputedStyle(childElement).getPropertyValue(property);
    parentElement.removeChild(childElement);
    document.body.removeChild(parentElement);
    return {parent: parentComputedValue, child: childComputedValue};
}

function getParentAndChildComputedStylesString(property, parentValue, childValue) 
{
    var styles = getParentAndChildComputedStyles(property, parentValue, childValue);
    return "parent: " + styles.parent + ", child: " + styles.child;
}

function getChildComputedStyle(property, parentValue, childValue) 
{
    var styles = getParentAndChildComputedStyles(property, parentValue, childValue);
    return styles.child;
}

function testSpecifiedProperty(property, value, expectedValue)
{
    shouldBeEqualToString('getCSSText("' + property + '", "' + value + '")', expectedValue);
}

function testComputedProperty(property, value, expectedValue)
{
    shouldBeEqualToString('getComputedStyleValue("' + property + '", "' + value + '")', expectedValue);
}

function testNotInheritedChildProperty(property, parentValue, childValue, expectedChildValue)
{
    shouldBeEqualToString('getChildComputedStyle("' + property + '", "' + parentValue + '", "' + childValue + '")', expectedChildValue);
}

function applyToEachArglist(testFunction, arglists)
{
    arglists.forEach(function(arglist, i, a) {testFunction.apply(null, arglist);});
}
