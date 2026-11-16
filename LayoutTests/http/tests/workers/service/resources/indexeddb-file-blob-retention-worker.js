importScripts("indexeddb-file.js");

let heldPort;

onmessage = (event) => {
    if (event.data instanceof File) {
        readPostedFile(event.data).then((result) => event.source.postMessage(result));
        return;
    }

    if (event.data.command === "hold-port") {
        heldPort = event.ports[0];
        event.source.postMessage("holding-port");
        return;
    }

    if (event.data.command === "read-port") {
        heldPort.onmessage = async (portEvent) => event.source.postMessage(await readPostedFile(portEvent.data));
        return;
    }

    if (event.data.command === "broadcast-file") {
        fileFromIndexedDB("broadcast-after-release-db").then((file) => {
            broadcastChannel.postMessage(file);
            event.source.postMessage("broadcast-file-posted");
        });
        return;
    }

    if (event.data.command === "block") {
        event.source.postMessage("blocking");
        const deadline = Date.now() + event.data.milliseconds;
        while (Date.now() < deadline) { }
    }
};

const broadcastChannel = new BroadcastChannel("indexeddb-file-broadcast-channel");
broadcastChannel.onmessage = async (event) => broadcastChannel.postMessage(await readPostedFile(event.data));
