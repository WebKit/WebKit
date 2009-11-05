// Inspected Page functions.

function doit()
{
    function callback(result)
    {
        for (var i = 0; i < result.length; ++i)
            output(result[i]);
        notifyDone();
    }
    evaluateInWebInspector("frontend_doitAndDump", callback);
}

// Frontend functions.

function frontend_dumpSyntaxHighlight(str, highlighterConstructor)
{
    var node = document.createElement("span");
    node.textContent = str;
    var javascriptSyntaxHighlighter = new highlighterConstructor(null, null);
    javascriptSyntaxHighlighter.syntaxHighlightNode(node);
    var node_parts = [];
    for (var i = 0; i < node.childNodes.length; i++) {
        if (node.childNodes[i].getAttribute) {
            node_parts.push(node.childNodes[i].getAttribute("class"));
        } else {
            node_parts.push("*");
        }
    }
    return node_parts.join(",");
}
