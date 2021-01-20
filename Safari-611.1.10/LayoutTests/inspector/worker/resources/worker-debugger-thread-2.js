let testVariable = "worker thread 2";

onmessage = function(event) {
    if (event.data === "doWork") {
        setTimeout(workInThread2, 250);
        setTimeout(laterWorkInThread2, 250);
    }
}

function workInThread2() {
    Date.now();
}

function laterWorkInThread2() {
    Date.now();
}
