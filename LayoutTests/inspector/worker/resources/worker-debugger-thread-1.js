let testVariable = "worker thread 1";

onmessage = function(event) {
    if (event.data === "doWork")
        setTimeout(workInThread1, 0)
}

function workInThread1() {
    foo();
}

function foo() {
    debugger;
}
