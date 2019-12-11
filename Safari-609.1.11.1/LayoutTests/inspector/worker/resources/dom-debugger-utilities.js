window.worker = null;

TestPage.registerInitializer(() => {
    if (!InspectorTest.Worker)
        InspectorTest.Worker = {};

    if (!InspectorTest.Worker.DOMDebugger)
        InspectorTest.Worker.DOMDebugger = {};

    InspectorTest.Worker.DOMDebugger.createWorkerTarget = function(callback) {
        let listener = WI.targetManager.addEventListener(WI.TargetManager.Event.TargetAdded, (event) => {
            let {target} = event.data;
            if (target.type !== WI.TargetType.Worker)
                return;

            WI.targetManager.removeEventListener(WI.TargetManager.Event.TargetAdded, listener);
            callback(target);
        });

        InspectorTest.evaluateInPage(`window.worker = new Worker("resources/worker-dom-debugger.js");`)
        .catch((error) => {
            InspectorTest.fail(error);
            InspectorTest.completeTest();
        });
    }
});
