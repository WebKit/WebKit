function clearStorages() {
    sessionStorage.clear();
    sessionStorage.setItem("foo", "bar");

    localStorage.clear();
    localStorage.setItem("foo", "bar");
}

TestPage.registerInitializer(() => {
    InspectorTest.Storage = {};

    InspectorTest.Storage.logEntries = async function(storage) {
        InspectorTest.newline();
        InspectorTest.log("Getting DOM storage entries...");
        let [error, entries] = await promisify((callback) => { storage.getEntries(callback); });
        InspectorTest.assert(!error, error);
        InspectorTest.json(entries);
        InspectorTest.newline();
    };
});
