var initialize_SyntaxHighlight = function() {

InspectorTest.dumpSyntaxHighlight = function(str, mimeType)
{
    var node = document.createElement("span");
    node.textContent = str;
    var javascriptSyntaxHighlighter = new WebInspector.DOMSyntaxHighlighter(mimeType);
    javascriptSyntaxHighlighter.syntaxHighlightNode(node);
    var node_parts = [];
    for (var i = 0; i < node.childNodes.length; i++) {
        if (node.childNodes[i].getAttribute) {
            node_parts.push(node.childNodes[i].getAttribute("class"));
        } else {
            node_parts.push("*");
        }
    }
    InspectorTest.addResult(str + ": " + node_parts.join(","));
};

};
