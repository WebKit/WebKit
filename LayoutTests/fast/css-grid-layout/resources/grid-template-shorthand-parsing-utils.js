function testGridDefinitionsValues(element, computedColumnsValue, computedRowsValue, computedAreasValue)
{
    window.element = element;
    var elementID = element.id || "element";
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-columns')", computedColumnsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-rows')", computedRowsValue);
    shouldBeEqualToString("window.getComputedStyle(" + elementID + ", '').getPropertyValue('-webkit-grid-template-areas')", computedAreasValue);
}

function testGridDefinitionsSetJSValues(shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, jsColumnsValue, jsRowsValue, jsAreasValue)
{
    checkGridDefinitionsSetJSValues(true, shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, jsColumnsValue, jsRowsValue, jsAreasValue);
}

function testNonGridDefinitionsSetJSValues(shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, jsColumnsValue, jsRowsValue, jsAreasValue)
{
    checkGridDefinitionsSetJSValues(false, shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, jsColumnsValue, jsRowsValue, jsAreasValue);
}

function checkGridDefinitionsSetJSValues(useGrid, shorthandValue, computedColumnsValue, computedRowsValue, computedAreasValue, jsColumnsValue, jsRowsValue, jsAreasValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);
    if (useGrid) {
        element.style.display = "-webkit-grid";
        element.style.width = "800px";
        element.style.height = "600px";
        element.style.justifyContent = "start";
        element.style.alignContent = "start";
    }

    element.style.font = "10px Ahem"; // Used to resolve em font consistently.
    element.style.webkitGridTemplate = shorthandValue;
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-columns')", computedColumnsValue);
    shouldBeEqualToString("element.style.webkitGridTemplateColumns", jsColumnsValue || computedColumnsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-rows')", computedRowsValue);
    shouldBeEqualToString("element.style.webkitGridTemplateRows", jsRowsValue || computedRowsValue);
    shouldBeEqualToString("getComputedStyle(element, '').getPropertyValue('-webkit-grid-template-areas')", computedAreasValue);
    shouldBeEqualToString("element.style.webkitGridTemplateAreas", jsAreasValue || computedAreasValue);
    document.body.removeChild(element);

}

function testGridDefinitionsSetBadJSValues(shorthandValue)
{
    window.element = document.createElement("div");
    document.body.appendChild(element);

    element.style.webkitGridTemplate = shorthandValue;
    // We can't use testSetJSValues as element.style.webkitGridTemplateRows returns "".
    testGridDefinitionsValues(element, "none", "none", "none");
    document.body.removeChild(element);
}
