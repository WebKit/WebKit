// Valid values for both shape-inside and shape-outside. Two values are specified when the shape property value 
// differs from the specified value.
var validShapeValues = [
    "auto",
    "rectangle(10px, 20px, 30px, 40px)", 
    "rectangle(10px, 20px, 30px, 40px, 5px)", 
    "rectangle(10px, 20px, 30px, 40px, 5px, 10px)",

    "inset-rectangle(10px, 20px, 30px, 40px)",
    "inset-rectangle(10px, 20px, 30px, 40px, 5px)",
    "inset-rectangle(10px, 20px, 30px, 40px, 5px, 10px)",

    "circle(10px, 20px, 30px)",

    "ellipse(10px, 20px, 30px, 40px)",

    ["polygon(10px 20px, 30px 40px, 40px 50px)", "polygon(nonzero, 10px 20px, 30px 40px, 40px 50px)"],
    ["polygon(evenodd, 10px 20px, 30px 40px, 40px 50px)", "polygon(evenodd, 10px 20px, 30px 40px, 40px 50px)"],
    ["polygon(nonzero, 10px 20px, 30px 40px, 40px 50px)", "polygon(nonzero, 10px 20px, 30px 40px, 40px 50px)"]
];

// Invalid values for both shape-inside and shape-outside. When an invalid shape value is specified, the 
// shape property's computed value is the same as its default.
var invalidShapeValues = [
    "calc()",
    "none",

    "rectangle()",
    "rectangle(10px)",
    "rectangle(10px, 10px)",
    "rectangle(10px, 20px, 30px)",
    "rectangle(10px 20px 30px 40px)",
    "rectangle(10px, 20px, 30px, 40px, 50px, 60px, 70px)",

    "inset-rectangle()",
    "inset-rectangle(10px)",
    "inset-rectangle(10px, 10px)",
    "inset-rectangle(10px, 20px, 30px)",
    "inset-rectangle(10px 20px 30px 40px)",
    "inset-rectangle(10px, 20px, 30px, 40px, 50px, 60px, 70px)",

    "circle()",
    "circle(10px)",
    "circle(10px, 20px)",
    "circle(10px 20px 30px)",
    "circle(10px, 20px, 30px, 40px)",

    "ellipse()",
    "ellipse(10px)",
    "ellipse(10px, 20px)",
    "ellipse(10px, 20px, 30px)",
    "ellipse(10px 20px 30px 40px)",

    "polygon()",
    "polygon(evenodd 10px 20px, 30px 40px, 40px 50px)",
    "polygon(nonzero 10px 20px, 30px 40px, 40px 50px)",
    "polygon(nonzero)",
    "polygon(evenodd)",
    "polygon(10px)",
    "polygon(nonzero,10px)",
    "polygon(evenodd,12px)",
    "polygon(10px, 20px, 30px, 40px, 40px, 50px)",
];

// Valid length values for shape-margin and shape-padding.
var validShapeLengths = [
    "1.5ex",
    "2em",
    "2.5in",
    "3cm",
    "3.5mm",
    "4pt",
    "4.5pc",
    "5px"
];

// Invalid length values for shape-margin and shape-padding.
var invalidShapeLengths = [
    "-5px",
    "auto",
    "120%",
    "\'string\'"
];

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

function testShapeSpecifiedProperty(property, value, expectedValue)
{
    shouldBeEqualToString('getCSSText("' + property + '", "' + value + '")', expectedValue);
}

function testShapeComputedProperty(property, value, expectedValue)
{
    shouldBeEqualToString('getComputedStyleValue("' + property + '", "' + value + '")', expectedValue);
}

function testNotInheritedShapeChildProperty(property, parentValue, childValue, expectedChildValue)
{
    shouldBeEqualToString('getChildComputedStyle("' + property + '", "' + parentValue + '", "' + childValue + '")', expectedChildValue);
}

// Need to remove the base URL to avoid having local paths in the expected results.
function removeBaseURL(src) {
    var urlRegexp = /url\(([^\)]*)\)/g;
    return src.replace(urlRegexp, function(match, url) {
        return "url(" + url.substr(url.lastIndexOf("/") + 1) + ")";
    });
}

function testLocalURLShapeProperty(property, value, expected) 
{
    shouldBeEqualToString('removeBaseURL(getCSSText("' + property + '", "' + value + '"))', expected);
    shouldBeEqualToString('removeBaseURL(getComputedStyleValue("' + property + '", "' + value + '"))', expected);
}

function testShapePropertyParsingFailure(property, value, defaultComputedStyle) 
{
    shouldBeEqualToString('getCSSText("' + property + '", "' + value + '")', '');
    shouldBeEqualToString('getComputedStyleValue("' + property + '", "' + value + '")', defaultComputedStyle);
}

function testNotInheritedShapeProperty(property, parentValue, childValue, expectedValue)
{
    shouldBeEqualToString('getParentAndChildComputedStylesString("' + property + '", "' + parentValue + '", "' + childValue + '")', expectedValue);
}

function applyToEachArglist(testFunction, arglists)
{
    arglists.forEach(function(arglist, i, a) {testFunction.apply(null, arglist);});
}
