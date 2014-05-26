function testGridDefinitionsValues(element, columnsValue, rowsValue, areasValue, autoFlowValue, autoColumnsValue, autoRowsValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-columns')", columnsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-rows')", rowsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-areas')", areasValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-auto-flow')", autoFlowValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-auto-columns')", autoColumnsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-auto-rows')", autoRowsValue);
}

function testGridDefinitionsSetJSValues(shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, computedAutoFlowValue, computedAutoColumnsValue, computedAutoRowsValue, jsColumnsValue, jsRowsValue, jsAreasValue, jsAutoFlowValue, jsAutoColumnsValue, jsAutoRowsValue)
{
    checkGridDefinitionsSetJSValues(true, shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, computedAutoFlowValue, computedAutoColumnsValue, computedAutoRowsValue, jsColumnsValue, jsRowsValue, jsAreasValue, jsAutoFlowValue, jsAutoColumnsValue, jsAutoRowsValue);
}

function testNonGridDefinitionsSetJSValues(shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, computedAutoFlowValue, computedAutoColumnsValue, computedAutoRowsValue, jsColumnsValue, jsRowsValue, jsAreasValue, jsAutoFlowValue, jsAutoColumnsValue, jsAutoRowValue)
{
    checkGridDefinitionsSetJSValues(false, shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, computedAutoFlowValue, computedAutoColumnsValue, computedAutoRowsValue, jsColumnsValue, jsRowsValue, jsAreasValue, jsAutoFlowValue, jsAutoColumnsValue, jsAutoRowValue);
}

function checkGridDefinitionsSetJSValues(useGrid, shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, computedAutoFlowValue, computedAutoColumnsValue, computedAutoRowsValue, jsColumnsValue, jsRowsValue, jsAreasValue, jsAutoFlowValue, jsAutoColumnsValue, jsAutoRowsValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    if (useGrid) {
        element.style.display = "-webkit-grid";
        element.style.width = "800px";
        element.style.height = "600px";
    }
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.webkitGrid = shorthandValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", computedColumnsValue);
    shouldBeEqualToString("element.style.webkitGridTemplateColumns", jsColumnsValue || computedColumnsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", computedRowsValue);
    shouldBeEqualToString("element.style.webkitGridTemplateRows", jsRowsValue || computedRowsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-areas')", computedAreasValue);
    shouldBeEqualToString("element.style.webkitGridTemplateAreas", jsAreasValue || computedAreasValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-auto-flow')", computedAutoFlowValue);
    shouldBeEqualToString("element.style.webkitGridAutoFlow", jsAutoFlowValue || computedAutoFlowValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-auto-columns')", computedAutoColumnsValue);
    shouldBeEqualToString("element.style.webkitGridAutoColumns", jsAutoColumnsValue || computedAutoColumnsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-auto-rows')", computedAutoRowsValue);
    shouldBeEqualToString("element.style.webkitGridAutoRows", jsAutoRowsValue || computedAutoRowsValue);
    document.body.removeChild(element);
}

function testGridDefinitionsSetBadJSValues(shorthandValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.webkitGrid = shorthandValue;
    // We can't use testSetJSValues as element.style.webkitGridTemplateRows returns "".
    testGridDefinitionsValues(element, "none", "none", "none");
    document.body.removeChild(element);
}