(function() {

function checkColumnRowValues(gridItem, columnValue, rowValue)
{
    this.gridItem = gridItem;
    gridItemId = gridItem.id ? gridItem.id : "gridItem";

    var gridColumnStartEndValues = columnValue.split("/")
    var gridColumnStartValue = gridColumnStartEndValues[0].trim();
    var gridColumnEndValue = gridColumnStartEndValues[1].trim();

    var gridRowStartEndValues = rowValue.split("/")
    var gridRowStartValue = gridRowStartEndValues[0].trim();
    var gridRowEndValue = gridRowStartEndValues[1].trim();

    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('-webkit-grid-column')", columnValue);
    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('-webkit-grid-column-start')", gridColumnStartValue);
    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('-webkit-grid-column-end')", gridColumnEndValue);
    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('-webkit-grid-row')", rowValue);
    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('-webkit-grid-row-start')", gridRowStartValue);
    shouldBeEqualToString("getComputedStyle(" + gridItemId + ", '').getPropertyValue('-webkit-grid-row-end')", gridRowEndValue);
}

window.testColumnRowCSSParsing = function(id, columnValue, rowValue)
{
    var gridItem = document.getElementById(id);
    checkColumnRowValues(gridItem, columnValue, rowValue);
}

window.testColumnRowJSParsing = function(columnValue, rowValue, expectedColumnValue, expectedRowValue)
{
    var gridItem = document.createElement("div");
    var gridElement = document.getElementsByClassName("grid")[0];
    gridElement.appendChild(gridItem);
    gridItem.style.webkitGridColumn = columnValue;
    gridItem.style.webkitGridRow = rowValue;

    checkColumnRowValues(gridItem, expectedColumnValue ? expectedColumnValue : columnValue, expectedRowValue ? expectedRowValue : rowValue);

    gridElement.removeChild(gridItem);
}

window.testColumnRowInvalidJSParsing = function(columnValue, rowValue)
{
    var gridItem = document.createElement("div");
    document.body.appendChild(gridItem);
    gridItem.style.webkitGridColumn = columnValue;
    gridItem.style.webkitGridRow = rowValue;

    checkColumnRowValues(gridItem, "auto / auto", "auto / auto");

    document.body.removeChild(gridItem);
}

window.testColumnStartRowStartJSParsing = function(columnStartValue, rowStartValue, expectedColumnStartValue, expectedRowStartValue)
{
    var gridItem = document.createElement("div");
    var gridElement = document.getElementsByClassName("grid")[0];
    gridElement.appendChild(gridItem);
    gridItem.style.webkitGridColumnStart = columnStartValue;
    gridItem.style.webkitGridRowStart = rowStartValue;

    if (expectedColumnStartValue === undefined)
        expectedColumnStartValue = columnStartValue;
    if (expectedRowStartValue === undefined)
        expectedRowStartValue = rowStartValue;

    checkColumnRowValues(gridItem, expectedColumnStartValue + " / auto", expectedRowStartValue + " / auto");

    gridElement.removeChild(gridItem);
}

window.testColumnEndRowEndJSParsing = function(columnEndValue, rowEndValue, expectedColumnEndValue, expectedRowEndValue)
{
    var gridItem = document.createElement("div");
    var gridElement = document.getElementsByClassName("grid")[0];
    gridElement.appendChild(gridItem);
    gridItem.style.webkitGridColumnEnd = columnEndValue;
    gridItem.style.webkitGridRowEnd = rowEndValue;

    if (expectedColumnEndValue === undefined)
        expectedColumnEndValue = columnEndValue;
    if (expectedRowEndValue === undefined)
        expectedRowEndValue = rowEndValue;

    checkColumnRowValues(gridItem, "auto / " + expectedColumnEndValue, "auto / " + expectedRowEndValue);

    gridElement.removeChild(gridItem);
}

})();
