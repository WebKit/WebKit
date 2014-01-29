function testGridDefinitionsValues(element, columnValue, rowValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-definition-columns')", columnValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-definition-rows')", rowValue);
}

function testGridDefinitionsSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridDefinitionsSetJSValues(true, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function testNonGridDefinitionsSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridDefinitionsSetJSValues(false, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function checkGridDefinitionsSetJSValues(useGrid, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    if (useGrid) {
        element.style.display = "grid";
        element.style.width = "800px";
        element.style.height = "600px";
    }
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.webkitGridDefinitionColumns = columnValue;
    element.style.webkitGridDefinitionRows = rowValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-columns')", computedColumnValue || columnValue);
    shouldBeEqualToString("element.style.webkitGridDefinitionColumns", jsColumnValue || columnValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-definition-rows')", computedRowValue || rowValue);
    shouldBeEqualToString("element.style.webkitGridDefinitionRows", jsRowValue || rowValue);
    document.body.removeChild(element);
}

function testGridDefinitionsSetBadJSValues(columnValue, rowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.webkitGridDefinitionColumns = columnValue;
    element.style.webkitGridDefinitionRows = rowValue;
    // We can't use testSetJSValues as element.style.webkitGridDefinitionRows returns "".
    testGridDefinitionsValues(element, "none", "none");
    document.body.removeChild(element);
}
