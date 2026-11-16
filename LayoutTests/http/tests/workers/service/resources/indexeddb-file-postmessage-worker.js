function requestPromise(request)
{
    return new Promise((resolve, reject) => {
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
}

async function fileFromIndexedDB()
{
    const openRequest = indexedDB.open("service-worker-posted-file-db", 1);
    openRequest.onupgradeneeded = () => openRequest.result.createObjectStore("files");
    const database = await requestPromise(openRequest);

    const writeTransaction = database.transaction("files", "readwrite");
    writeTransaction.objectStore("files").put(new File(["contents stored in IndexedDB"], "stored.txt", { type: "text/plain" }), "key");
    await new Promise((resolve) => writeTransaction.oncomplete = resolve);

    return requestPromise(database.transaction("files").objectStore("files").get("key"));
}

onmessage = async (event) => {
    try {
        event.source.postMessage(await fileFromIndexedDB());
    } catch (error) {
        event.source.postMessage("failed to read from IndexedDB: " + error);
    }
};
