// Frontend functions.

function frontend_expandDOMSubtree(node)
{
    function processChildren(children)
    {
       for (var i = 0; children && i < children.length; ++i)
           frontend_expandDOMSubtree(children[i]);
    }
    WebInspector.domAgent.getChildNodesAsync(node, processChildren);
}

function frontend_nodeForId(idValue)
{
    var innerMapping = WebInspector.domAgent._idToDOMNode;
    for (var nodeId in innerMapping) {
        var node = innerMapping[nodeId];
        if (node.getAttribute("id") === idValue)
            return node;
    }
    return null;
}
