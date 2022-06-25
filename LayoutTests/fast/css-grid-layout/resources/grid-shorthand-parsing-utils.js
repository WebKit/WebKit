function testGridDefinitionsValues(element, columnsValue, rowsValue, areasValue, autoFlowValue, autoColumnsValue, autoRowsValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-template-columns')", columnsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-template-rows')", rowsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-template-areas')", areasValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-auto-flow')", autoFlowValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-auto-columns')", autoColumnsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('grid-auto-rows')", autoRowsValue);
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
        element.style.display = "grid";
        element.style.width = "800px";
        element.style.height = "600px";
    }
    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.grid = shorthandValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-template-columns')", computedColumnsValue);
    shouldBeEqualToString("element.style.gridTemplateColumns", jsColumnsValue || computedColumnsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-template-rows')", computedRowsValue);
    shouldBeEqualToString("element.style.gridTemplateRows", jsRowsValue || computedRowsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-template-areas')", computedAreasValue);
    shouldBeEqualToString("element.style.gridTemplateAreas", jsAreasValue || computedAreasValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-auto-flow')", computedAutoFlowValue);
    shouldBeEqualToString("element.style.gridAutoFlow", jsAutoFlowValue || computedAutoFlowValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-auto-columns')", computedAutoColumnsValue);
    shouldBeEqualToString("element.style.gridAutoColumns", jsAutoColumnsValue || computedAutoColumnsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('grid-auto-rows')", computedAutoRowsValue);
    shouldBeEqualToString("element.style.gridAutoRows", jsAutoRowsValue || computedAutoRowsValue);
    document.body.removeChild(element);
}

function testGridDefinitionsSetBadJSValues(shorthandValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    element.style.grid = shorthandValue;
    // We can't use testSetJSValues as element.style.gridTemplateRows returns "".
    testGridDefinitionsValues(element, "none", "none", "none");
    document.body.removeChild(element);
}