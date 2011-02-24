function initialize_HighlighterTests()
{

InspectorTest.dumpTextModel = function(textModel)
{
    var lines = [];
    for (var i = 0; i < textModel.linesCount; ++i) {
        var line = textModel.line(i);
        var highlight = textModel.getAttribute(i, "highlight");
        var result = (i + 1) + " : " + line + " :";
        if (highlight) {
            for (var j = 0; j < line.length; ++j)
                if (highlight[j])
                    result += " " + highlight[j].tokenType + "[" + j + "-" + (j + highlight[j].length) + "]";
        } else
            result += " null";
        lines.push(result);
    }
    InspectorTest.addResult(lines.join("\n"));
};

}
