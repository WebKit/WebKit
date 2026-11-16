TestPage.registerInitializer(function() {
    // `WI.layerTreeManager` predates promises, so wrap the two commands these tests need.
    window.getLayersForNode = function(node) {
        return new Promise((resolve) => WI.layerTreeManager.layersForNode(node, resolve));
    };

    window.getReasonsForCompositingLayer = function(layer) {
        return new Promise((resolve) => WI.layerTreeManager.reasonsForCompositingLayer(layer, resolve));
    };

    // `LayerTree.layersForNode` walks down from the node it is given, so every test starts from
    // the document. Requesting it here also primes the DOM agent that owns the node ids the
    // returned layers refer to, without which those ids cannot be resolved back to elements.
    window.getDocumentAndLayers = async function() {
        let documentNode = await WI.domManager.requestDocument();
        InspectorTest.assert(documentNode, "Should have a document node.");
        return {documentNode, layers: await getLayersForNode(documentNode)};
    };

    // A layer names the element that generates it by node id. Resolving the element a test cares
    // about through the same DOM agent is what keeps both sides in one node-id space.
    window.getNodeForSelector = async function(documentNode, selector) {
        let nodeId = await documentNode.querySelector(selector);
        return WI.domManager.nodeForId(nodeId);
    };
});
