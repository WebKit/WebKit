window.ShapeInfoHelper = (function() {
    function shapeOutsideInfo(selector, callback) {
        InspectorTest.sendCommand("DOM.getDocument", {}, onGotDocument);

        function onGotDocument(msg) {
            InspectorTest.checkForError(msg);
            var node = msg.result.root;
            InspectorTest.sendCommand("DOM.querySelector",
            {
                "nodeId": node.nodeId,
                "selector": selector
            },
            onQuerySelector);
        }

        function onQuerySelector(msg) {
            InspectorTest.checkForError(msg);
            var node = msg.result;
            var highlightConfig = {
                showInfo: true,
                contentColor: {r: 255, g: 255, b: 255},
                paddingColor: {r: 255, g: 255, b: 255},
                borderColor: {r: 255, g: 255, b: 255},
                marginColor: {r: 255, g: 255, b: 255},
            };
            InspectorTest.sendCommand("DOM.highlightNode",
            {
                "nodeId": node.nodeId,
                "highlightConfig": highlightConfig
            },
            onHighlightNode);
        }

        function onHighlightNode(msg) {
            InspectorTest.checkForError(msg);
            InspectorTest.sendCommand("Runtime.evaluate",
            {
                "expression": "window.internals.inspectorHighlightObject()"
            },
            onRuntimeEvaluate);
        }

        function onRuntimeEvaluate(msg) {
            InspectorTest.checkForError(msg);
            var value = JSON.parse(msg.result.result.value);
            callback(value.elementInfo.shapeOutsideInfo);
        }
    }

    function pathsRoughlyEqual(actual, expected) {
        function coordinatesRoughlyEqual(actual, expected) {
            // Some platforms may only store integer path points, so the potential
            // differences between correct paths can be roughly half a unit
            return (typeof actual === 'number'
                && typeof expected === 'number'
                && Math.abs(actual - expected) < 0.5);
        }

        var expectedSubpathStart = 0, ei, ai;
        for (var ei = 0, ai = 0; ai < actual.length && ei < expected.length; ai++, ei++) {
            if (expected[ei] === 'M')
                expectedSubpathStart = ei;

            if (actual[ai] === expected[ei]
                || coordinatesRoughlyEqual(actual[ai], expected[ei]))
                continue;

            // Extrapolate the missing line to command if it is missing from actual.
            // The choice to close the path with an explicit line to command is
            // platform dependent.
            if (actual[ai] === 'Z'
                && expected[ei] === 'L'
                && coordinatesRoughlyEqual(expected[expectedSubpathStart + 1], expected[ei + 1])
                && coordinatesRoughlyEqual(expected[expectedSubpathStart + 2], expected[ei + 2])) {
                ei += 2;
                ai--;
                continue;
            }
            return false;
        }
        return true;
    }

    function runShapeTest(testfn) {
        var body = [
            "InspectorTest.shapeOutsideInfo = " + shapeOutsideInfo.toString(),
            "InspectorTest.pathsRoughlyEqual = " + pathsRoughlyEqual.toString(),
            "(" + testfn.toString() + ")()"
        ];
        window.test = new Function(
            body.join(';\n')
        );
        runTest();
    }

    return {
        'runShapeTest' : runShapeTest
    }
})();
