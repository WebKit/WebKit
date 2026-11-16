const indexedDBFileContents = "contents stored in IndexedDB";

function indexedDBRequestPromise(request)
{
    return new Promise((resolve, reject) => {
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
}

async function fileFromIndexedDB(databaseName)
{
    const openRequest = indexedDB.open(databaseName, 1);
    openRequest.onupgradeneeded = () => openRequest.result.createObjectStore("files");
    const database = await indexedDBRequestPromise(openRequest);

    const writeTransaction = database.transaction("files", "readwrite");
    writeTransaction.objectStore("files").put(new File([indexedDBFileContents], "stored.txt", { type: "text/plain" }), "key");
    await new Promise((resolve) => writeTransaction.oncomplete = resolve);

    const file = await indexedDBRequestPromise(database.transaction("files").objectStore("files").get("key"));
    database.close();
    return file;
}

async function readPostedFile(data)
{
    if (!(data instanceof File))
        return "did not receive a File, received " + data;
    try {
        return "read '" + await data.text() + "' from the posted File";
    } catch (error) {
        return "failed to read the posted File: " + error;
    }
}

function blockServiceWorker(worker)
{
    return new Promise((resolve) => {
        navigator.serviceWorker.addEventListener("message", resolve, { once: true });
        worker.postMessage({ command: "block", milliseconds: 1000 });
    });
}

async function releaseFile()
{
    for (let i = 0; i < 10; ++i) {
        gc();
        await new Promise((resolve) => setTimeout(resolve, 0));
    }
}
