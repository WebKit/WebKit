function checkValues(element, property, propertyID, value, computedValue)
{
    window.element = element;
    var elementID = element.id || "element";
    assert_equals(eval('element.style.' + property), value, property + ' specified value is not what it should.');
    assert_equals(eval("window.getComputedStyle(" + elementID + ", '').getPropertyValue('" + propertyID + "')"), computedValue, property + " is not what is should.");
}

function checkBadValues(element, property, propertyID, value)
{
    var elementID = element.id || "element";
    var initialValue = eval("window.getComputedStyle(" + elementID + " , '').getPropertyValue('" + propertyID + "')");
    element.style[property] = value;
    checkValues(element, property, propertyID, "", initialValue);
}

function checkInitialValues(element, property, propertyID, value, initial)
{
    element.style[property] = value;
    checkValues(element, property, propertyID, value, value);
    element.style[property] = "initial";
    checkValues(element, property, propertyID, "initial", initial);
}

function checkInheritValues(property, propertyID, value)
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style[property] = value;
    checkValues(parentElement, property, propertyID, value, value);

    var element = document.createElement("div");
    parentElement.appendChild(element);
    element.style[property] = "inherit";
    checkValues(element, property, propertyID, "inherit", value);
}

function checkLegacyValues(property, propertyID, value)
{
    var parentElement = document.createElement("div");
    document.body.appendChild(parentElement);
    parentElement.style[property] = value;
    checkValues(parentElement, property, propertyID, value, value);

    var element = document.createElement("div");
    parentElement.appendChild(element);
    checkValues(element, property, propertyID, "", value);
}

function checkSupportedValues(elementID, property)
{
    var value = eval("window.getComputedStyle(" + elementID + " , '').getPropertyValue('" + property + "')");
    var value1 = eval("window.getComputedStyle(" + elementID + " , '')");
    shouldBeTrue("CSS.supports('" + property + "', '" + value + "')");
}
