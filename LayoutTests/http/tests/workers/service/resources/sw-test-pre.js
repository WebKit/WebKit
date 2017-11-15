// All SW tests are async and text-only
if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
}

function log(msg)
{
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

function finishSWTest()
{
    if (window.testRunner)
        testRunner.notifyDone();
}

async function interceptedFrame(workerURL, scopeURL)
{
    var registration = await navigator.serviceWorker.register(workerURL, { scope : scopeURL });
    await new Promise(resolve => {
        if (registration.active)
            resolve();
        worker = registration.installing;
        if (worker.state === "activated")
            resolve();
        worker.addEventListener("statechange", () => {
            if (worker.state === "activated")
                resolve();
        });
    });

    return await new Promise((resolve) => {
        var frame = document.createElement('iframe');
        frame.src = scopeURL;
        frame.onload = function() { resolve(frame); };
        document.body.appendChild(frame);
    });
}
