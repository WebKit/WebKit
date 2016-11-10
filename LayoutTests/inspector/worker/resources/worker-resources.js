importScripts("resource-utilities.js");

function importScript() {
    importScripts("worker-import-1.js");
}

onmessage = function(event) {
    if (event.data === "loadResourceXHR")
        loadResourceXHR();
    else if (event.data === "loadResourceFetch")
        loadResourceFetch();
    else if (event.data === "importScript")
        importScript();
}

postMessage("ready");
