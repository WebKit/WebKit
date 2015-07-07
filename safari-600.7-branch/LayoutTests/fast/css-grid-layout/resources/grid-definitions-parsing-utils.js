function testGridTemplatesValues(element, columnValue, rowValue, computedColumnValue, computedRowValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-columns')", computedColumnValue || columnValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-rows')", computedRowValue || rowValue);
}

function testGridTemplatesSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridTemplatesSetJSValues(true, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function testNonGridTemplatesSetJSValues(columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    checkGridTemplatesSetJSValues(false, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue);
}

function checkGridTemplatesSetJSValues(useGrid, columnValue, rowValue, computedColumnValue, computedRowValue, jsColumnValue, jsRowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    if (useGrid) {
        element.style.display = "-webkit-grid";
        element.style.width = "800px";
        element.style.height = "600px";
    }
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.webkitGridTemplateColumns = columnValue;
    element.style.webkitGridTemplateRows = rowValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", computedColumnValue || columnValue);
    shouldBeEqualToString("element.style.webkitGridTemplateColumns", jsColumnValue || columnValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", computedRowValue || rowValue);
    shouldBeEqualToString("element.style.webkitGridTemplateRows", jsRowValue || rowValue);
    document.body.removeChild(element);
}

function testGridTemplatesSetBadJSValues(columnValue, rowValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.webkitGridTemplateColumns = columnValue;
    element.style.webkitGridTemplateRows = rowValue;
    // We can't use testSetJSValues as element.style.webkitGridTemplateRows returns "".
    testGridTemplatesValues(element, "none", "none");
    document.body.removeChild(element);
}
