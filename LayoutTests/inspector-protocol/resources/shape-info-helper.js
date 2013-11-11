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

    function runShapeTest(testfn) {
        var body = [
            "InspectorTest.shapeOutsideInfo = " + shapeOutsideInfo.toString(),
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
