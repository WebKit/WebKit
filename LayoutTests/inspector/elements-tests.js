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
