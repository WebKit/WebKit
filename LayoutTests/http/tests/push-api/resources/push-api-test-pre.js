if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function finishPushAPITest()
{
    if (typeof window !== 'undefined' && window.parent !== window) {
        window.parent.postMessage('finishPushAPITest', '*');
        return;
    }

    if (window.testRunner)
        testRunner.notifyDone();
}

function waitForState(worker, state)
{
    if (!worker || worker.state == undefined)
        return Promise.reject(new Error('wait_for_state must be passed a ServiceWorker'));

    if (worker.state === state)
        return Promise.resolve(state);

    return new Promise(function(resolve) {
      worker.addEventListener('statechange', function() {
          if (worker.state === state)
            resolve(state);
        });
    });
}

function log(msg)
{
    if (typeof window !== 'undefined' && window.parent !== window) {
        window.parent.postMessage(msg, '*');
        return;
    }

    let console = document.getElementById("console");
    if (!console) {
        console = document.createElement("div");
        console.id = "console";
        document.body.appendChild(console);
    }
    let span = document.createElement("span");
    span.innerHTML = msg + "<br>";
    console.appendChild(span);
}
