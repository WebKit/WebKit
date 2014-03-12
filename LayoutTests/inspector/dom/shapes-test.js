InspectorTestProxy.registerInitializer(function() {

InspectorTest.Shapes = {
    getShapeOutsideInfoForSelector: function(selector, callback)
    {
        WebInspector.domTreeManager.requestDocument(requestDocumentCallback);

        function requestDocumentCallback(node) {
            InspectorTest.assert(node instanceof WebInspector.DOMNode, "Unexpected argument to requestDocument callback.")

            DOMAgent.querySelector.invoke({nodeId: node.id, selector: selector}, querySelectorCallback);
        }

        function querySelectorCallback(error, nodeId) {
            InspectorTest.assert(!error, "Error in callback for DOMAgent.querySelector");
            InspectorTest.assert(nodeId, "Invalid argument to callback for DOMAgent.querySelector");

            var highlightConfig = {
                showInfo: true,
                contentColor: {r: 255, g: 255, b: 255},
                paddingColor: {r: 255, g: 255, b: 255},
                borderColor: {r: 255, g: 255, b: 255},
                marginColor: {r: 255, g: 255, b: 255},
            };
            DOMAgent.highlightNode.invoke({nodeId: nodeId, highlightConfig: highlightConfig}, highlightNodeCallback);
        }

        function highlightNodeCallback(error) {
            InspectorTest.assert(!error, "Error in callback for DOMAgent.highlightNode");

            InspectorTest.evaluateInPage("window.internals.inspectorHighlightObject()", receivedHighlightObject);
        }

        function receivedHighlightObject(error, payload, wasThrown) {
            console.assert(!error, "When evaluating code, received unexpected error:" + error);
            console.assert(!error, "When evaluating code, an exception was thrown:" + wasThrown);

            var data = JSON.parse(payload.value);
            callback(data.elementInfo.shapeOutsideInfo);
        }
    },

    assertPathsAreRoughlyEqual: function(actual, expected, message)
    {
        function coordinatesAreRoughlyEqual(a, b) {
            // Some platforms may only store integer path points, so the potential
            // differences between correct paths can be roughly half a unit.
            return typeof a === "number" && typeof b === "number" && Math.abs(a - b) < 0.5;
        }

        function pathsAreRoughlyEqual(actualPath, expectedPath) {
            var expectedSubpathStart = 0, ei, ai;
            for (var ei = 0, ai = 0; ai < actualPath.length && ei < expectedPath.length; ai++, ei++) {
                if (expectedPath[ei] === 'M')
                    expectedSubpathStart = ei;

                if (actualPath[ai] === expectedPath[ei]
                    || coordinatesAreRoughlyEqual(actualPath[ai], expectedPath[ei]))
                    continue;

                // Extrapolate the missing line to command if it is missing from actual.
                // The choice to close the path with an explicit line to command is
                // platform dependent.
                if (actualPath[ai] === 'Z'
                    && expectedPath[ei] === 'L'
                    && coordinatesAreRoughlyEqual(expectedPath[expectedSubpathStart + 1], expectedPath[ei + 1])
                    && coordinatesAreRoughlyEqual(expectedPath[expectedSubpathStart + 2], expectedPath[ei + 2])) {
                    ei += 2;
                    ai--;
                    continue;
                }
                return false;
            }
            return true;
        }

        function makeStringForPath(path) {
            return path.map(function(item) {
                if (typeof item === 'number')
                    return +item.toFixed(2);
                return item;
            }).join(' ');
        }

        var expectedPathString = makeStringForPath(expected);
        var actualPathString = makeStringForPath(actual);
        var assertionMessage = message + " \nexpected: " + expectedPathString + "\n actual: " + actualPathString;
        InspectorTest.assert(pathsAreRoughlyEqual(actual, expected), assertionMessage);
    }
}

});
