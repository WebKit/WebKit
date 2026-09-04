importScripts("indexeddb-file.js");

const broadcastChannel = new BroadcastChannel("indexeddb-file-broadcast-channel");
broadcastChannel.onmessage = async (event) => postMessage(await readPostedFile(event.data));

onmessage = (event) => {
    postMessage("blocking");
    const deadline = Date.now() + event.data.milliseconds;
    while (Date.now() < deadline) { }
};
