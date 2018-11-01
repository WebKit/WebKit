TestPage.registerInitializer(() => {
    let suite = null;

    InspectorTest.ObjectStore = {};

    InspectorTest.ObjectStore.TestObject = class TestObject {
        constructor(object) {
            this._object = object;
        }
        toJSON() {
            return this._object;
        }
    };

    InspectorTest.ObjectStore.basicObject1 = {a: 1};
    InspectorTest.ObjectStore.basicObject2 = {b: 2};

    InspectorTest.ObjectStore.createSuite = function(name) {
        suite = InspectorTest.createAsyncSuite(name);
        return suite;
    };

    InspectorTest.ObjectStore.createObjectStore = function(options = {}) {
        WI.ObjectStore.__testObjectStore = new WI.ObjectStore("__testing", options);
        return WI.ObjectStore.__testObjectStore;
    };

    InspectorTest.ObjectStore.add = async function(value, expected) {
        let result = await WI.ObjectStore.__testObjectStore.add(value);
        InspectorTest.assert(result === expected, `the key of the added item should be ${expected}, but is actually ${result}`);

        await InspectorTest.ObjectStore.logValues("add: ");
        return result;
    };

    InspectorTest.ObjectStore.addObject = async function(object, expected) {
        let result = await WI.ObjectStore.__testObjectStore.addObject(object);
        InspectorTest.assert(result === expected, `the key of the added item should be ${expected}, but is actually ${result}`);

        let resolved = WI.ObjectStore.__testObjectStore._resolveKeyPath(object);
        InspectorTest.assert(resolved.value === expected, `the resolved keyPath on the object should equal ${expected}, but is actually ${resolved.value}`);

        await InspectorTest.ObjectStore.logValues("addObject: ");
        return result;
    };

    InspectorTest.ObjectStore.delete = async function(value) {
        let result = await WI.ObjectStore.__testObjectStore.delete(value);
        InspectorTest.assert(result === undefined, `delete shouldn't return anything`);

        await InspectorTest.ObjectStore.logValues("delete: ");
    };

    InspectorTest.ObjectStore.deleteObject = async function(object) {
        let resolved = WI.ObjectStore.__testObjectStore._resolveKeyPath(object);
        InspectorTest.assert(resolved.key in resolved.object, `the resolved keyPath on the object should exist`);

        let result = await WI.ObjectStore.__testObjectStore.deleteObject(object);
        InspectorTest.assert(result === undefined, `deleteObject shouldn't return anything`);

        await InspectorTest.ObjectStore.logValues("deleteObject: ");
    };

    InspectorTest.ObjectStore.logValues = async function(prefix) {
        if (!WI.ObjectStore.__testObjectStore)
            return;

        prefix = prefix || "";
        let results = await WI.ObjectStore.__testObjectStore.getAll();
        InspectorTest.log(prefix + JSON.stringify(results));
    };

    InspectorTest.ObjectStore.wrapTest = function(name, func) {
        suite.addTestCase({
            name,
            async test() {
                InspectorTest.assert(!WI.ObjectStore.__testObjectStore, "__testObjectStore should be deleted after each test");

                await func();
                await InspectorTest.ObjectStore.logValues();

                delete WI.ObjectStore.__testObjectStore;

                if (WI.ObjectStore._database) {
                    WI.ObjectStore._database.close();
                    WI.ObjectStore._database = null;
                }

                await new Promise((resolve, reject) => {
                    let deleteDatabaseRequest = indexedDB.deleteDatabase(WI.ObjectStore._databaseName);
                    deleteDatabaseRequest.addEventListener("success", resolve);
                    deleteDatabaseRequest.addEventListener("error", reject);
                });
            },
        });
    };
});
