TestPage.registerInitializer(() => {
    window.HeapTest = {};

    let workerProxy = WI.HeapSnapshotWorkerProxy.singleton();

    // Documents of earlier tests can still be alive in this process and appear in this test's snapshots, so each load
    // uses its own marker class names.
    const markerSuffix = Math.random().toString(36).slice(2);

    HeapTest.markerClassName = function(prefix) {
        return `${prefix}_${markerSuffix}`;
    };

    HeapTest.processIdentifier = function(target) {
        return target.identifier.split("-").pop();
    };

    HeapTest.evaluate = async function(target, expression) {
        let response = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test", returnByValue: true});
        if (response.wasThrown)
            throw new Error(`Evaluating "${expression}" threw: ${response.result.description}`);
        return response.result.value;
    };

    HeapTest.takeSnapshot = async function(target) {
        let {snapshotData} = await target.HeapAgent.snapshot();
        let {objectId, snapshot: serializedSnapshot} = await new Promise((resolve) => workerProxy.createSnapshot(target.identifier, snapshotData, resolve));
        let snapshot = WI.HeapSnapshotProxy.deserialize(target, objectId, serializedSnapshot);
        snapshot.snapshotStringData = snapshotData;
        return snapshot;
    };

    HeapTest.objectCount = function(snapshot, className) {
        return snapshot.categories.get(className)?.objectCount ?? 0;
    };

    HeapTest.instances = function(snapshot, className) {
        return new Promise((resolve) => snapshot.instancesWithClassName(className, resolve));
    };

    HeapTest.nodeIdentifiers = function(snapshot) {
        const nodeFieldCount = 4; // <nodeId>, <sizeInBytes>, <nodeClassNameIndex>, <flags>
        let {nodes} = JSON.parse(snapshot.snapshotStringData);
        let identifiers = [];
        for (let i = 0; i < nodes.length; i += nodeFieldCount)
            identifiers.push(nodes[i]);
        return identifiers;
    };

    // The node that the given node references through the named property.
    HeapTest.propertyNode = function(node, propertyName) {
        return new Promise((resolve) => {
            node.retainedNodes((nodes, edges) => {
                let edge = edges.find((edge) => edge.type === WI.HeapSnapshotEdgeProxy.EdgeType.Property && edge.data === propertyName);
                resolve(nodes.find((retainedNode) => retainedNode.id === edge?.toIdentifier) ?? null);
            });
        });
    };

    // Resolves with the error of a protocol command, or null if it succeeded.
    HeapTest.commandError = async function(promise) {
        try {
            await promise;
            return null;
        } catch (error) {
            return error;
        }
    };

    // Resolves with the next snapshot reported by Console.heapSnapshot, without adding it to the active recording.
    HeapTest.awaitConsoleHeapSnapshot = function() {
        return new Promise((resolve) => {
            let originalHeapSnapshotAdded = WI.timelineManager.heapSnapshotAdded;
            WI.timelineManager.heapSnapshotAdded = function(timestamp, snapshot) {
                WI.timelineManager.heapSnapshotAdded = originalHeapSnapshotAdded;
                resolve(snapshot);
            };
        });
    };

    // Resolves once each of the given targets has reported a full garbage collection.
    HeapTest.awaitFullGarbageCollection = function(targets) {
        let remainingTargets = new Set(targets);
        return new Promise((resolve) => {
            let listener = WI.heapManager.addEventListener(WI.HeapManager.Event.GarbageCollected, (event) => {
                if (event.data.collection.type !== WI.GarbageCollection.Type.Full)
                    return;

                remainingTargets.delete(event.data.target);
                if (remainingTargets.size)
                    return;

                WI.heapManager.removeEventListener(WI.HeapManager.Event.GarbageCollected, listener);
                resolve();
            });
        });
    };
});
