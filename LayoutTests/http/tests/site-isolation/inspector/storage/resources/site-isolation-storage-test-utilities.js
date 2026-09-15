TestPage.registerInitializer(() => {
    InspectorTest.SiteIsolationStorage = {};

    // getDOMStorageItems returns StorageMap (HashMap) order, which is not stable; sort so the
    // expectations file is deterministic.
    InspectorTest.SiteIsolationStorage.logEntries = async function(storage) {
        InspectorTest.newline();
        InspectorTest.log("Getting DOM storage entries...");
        let [error, entries] = await promisify((callback) => { storage.getEntries(callback); });
        InspectorTest.assert(!error, error);
        InspectorTest.json(entries.slice().sort((a, b) => a[0].localeCompare(b[0])));
        InspectorTest.newline();
    };

    InspectorTest.SiteIsolationStorage.readInFrame = async function(frameTarget, storageName) {
        let expression = `JSON.stringify(Object.fromEntries(Object.entries(${storageName})))`;
        let {result} = await frameTarget.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test", returnByValue: true});
        return JSON.parse(result.value);
    };

    InspectorTest.SiteIsolationStorage.evaluateInFrame = function(frameTarget, expression) {
        return frameTarget.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test"});
    };

    InspectorTest.SiteIsolationStorage.objectForOrigin = function(securityOrigin, isLocalStorage) {
        return WI.domStorageManager.domStorageObjects.find((domStorage) => {
            return domStorage.isLocalStorage() === isLocalStorage && domStorage.id.securityOrigin === securityOrigin;
        }) || null;
    };
});
