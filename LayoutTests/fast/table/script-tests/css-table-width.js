description("This test checks that the width style is applied correctly to CSS tables with respect to table paddings and borders.");

function computeCSSTableOffsetWidth(extraTableStyle)
{
    return computeCSSTableProperty('offsetWidth', extraTableStyle)
}

function computeCSSTableOffsetHeight(extraTableStyle)
{
    return computeCSSTableProperty('offsetHeight', extraTableStyle)
}

function computeCSSTableProperty(propertyName, extraTableStyle)
{
    var table = document.createElement("div");
    table.setAttribute("style", "display: table; " + extraTableStyle);

    var rowGroup = document.createElement("div");
    rowGroup.setAttribute("style", "display: table-row-group;");

    var row = document.createElement("div");
    row.setAttribute("style", "display: table-row;");

    var cell = document.createElement("div");
    cell.setAttribute("style", "display: table-cell;");

    var cellContent = document.createElement("div");
    cellContent.setAttribute("style", "width: 100px; height: 50px; background-color: #090;");

    document.body.appendChild(table);
    table.appendChild(rowGroup);
    rowGroup.appendChild(row);
    row.appendChild(cell);
    cell.appendChild(cellContent);

    var propertyValue = table[propertyName];

    document.body.removeChild(table);

    return propertyValue;
}

// separated borders

shouldEvaluateTo("computeCSSTableOffsetWidth('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px;')", 200+2+4+6+8);
shouldEvaluateTo("computeCSSTableOffsetWidth('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; direction: rtl;')", 200+2+4+6+8);

shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl;')", 150+10+30+50+70);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl; -webkit-text-orientation: upright; text-orientation: upright;')", 150+10+30+50+70);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl; direction: rtl;')", 150+10+30+50+70);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl; direction: rtl; -webkit-text-orientation: upright; text-orientation: upright;')", 150+10+30+50+70);

shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr;')", 150+10+30+50+70);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr; -webkit-text-orientation: upright; text-orientation: upright;')", 150+10+30+50+70);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr; direction: rtl;')", 150+10+30+50+70);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr; direction: rtl; -webkit-text-orientation: upright; text-orientation: upright;')", 150+10+30+50+70);

// collapsed borders

shouldEvaluateTo("computeCSSTableOffsetWidth('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse;')", 200+(2+4)/2);
shouldEvaluateTo("computeCSSTableOffsetWidth('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; direction: rtl;')", 200+(2+4)/2);

shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl;')", 150+(10+30)/2);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl; -webkit-text-orientation: upright; text-orientation: upright;')", 150+(10+30)/2);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl; direction: rtl;')", 150+(10+30)/2);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-rl; writing-mode: vertical-rl; direction: rtl; -webkit-text-orientation: upright; text-orientation: upright;')", 150+(10+30)/2);

shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr;')", 150+(10+30)/2);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr; -webkit-text-orientation: upright; text-orientation: upright;')", 150+(10+30)/2);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr; direction: rtl;')", 150+(10+30)/2);
shouldEvaluateTo("computeCSSTableOffsetHeight('width: 200px; height: 150px; border-style: solid; border-width: 10px 2px 30px 4px; padding: 50px 6px 70px 8px; border-collapse: collapse; -webkit-writing-mode: vertical-lr; writing-mode: vertical-lr; direction: rtl; -webkit-text-orientation: upright; text-orientation: upright;')", 150+(10+30)/2);
