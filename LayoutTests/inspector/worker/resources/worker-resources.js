importScripts("resource-utilities.js");

function importScript() {
    importScripts("worker-import-1.js");
}

onmessage = function(event) {
    if (event.data === "loadResourceXHR")
        loadResourceXHR("dataXHR.json");
    else if (event.data === "loadResourceFetch")
        loadResourceFetch("dataFetch.json");
    else if (event.data === "importScript")
        importScript();
}

postMessage("ready");
