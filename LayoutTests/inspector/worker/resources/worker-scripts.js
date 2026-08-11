function triggerImportScripts() {
    importScripts("worker-import-1.js");
}

function triggerEvalScript() {
    eval("function workerEval() { }\n//# sourceURL=worker-eval.js");
}

onmessage = function(event) {
    if (event.data === "triggerImportScripts")
        triggerImportScripts();
    else if (event.data === "triggerEvalScript")
        triggerEvalScript();
}
