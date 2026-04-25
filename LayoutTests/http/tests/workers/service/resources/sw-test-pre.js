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

function withFrame(scopeURL)
{
    return new Promise((resolve) => {
        let frame = document.createElement('iframe');
        frame.src = scopeURL;
        frame.onload = function() { resolve(frame); };
        document.body.appendChild(frame);
    });
}

function registerAndWaitForActive(workerURL, scopeURL)
{
    return navigator.serviceWorker.register(workerURL, { scope : scopeURL }).then(function(registration) {
        return new Promise(resolve => {
            if (registration.active)
                resolve(registration);
            worker = registration.installing;
            worker.addEventListener("statechange", () => {
                if (worker.state === "activated")
                    resolve(registration);
            });
        });
    });
}

async function interceptedFrame(workerURL, scopeURL)
{
    await registerAndWaitForActive(workerURL, scopeURL);
    return await withFrame(scopeURL);
}

function gc() {
    if (typeof GCController !== "undefined")
        GCController.collect();
    else {
        var gcRec = function (n) {
            if (n < 1)
                return {};
            var temp = {i: "ab" + i + (i / 100000)};
            temp += "foo";
            gcRec(n-1);
        };
        for (var i = 0; i < 1000; i++)
            gcRec(10);
    }
}

// Builds a hex string representation for an array-like input.
// "bytes" can be an Array of bytes, an ArrayBuffer, or any TypedArray.
// The output looks like this:
//    ab034c99
function bytesToHexString(bytes)
{
    if (!bytes)
        return null;

    bytes = new Uint8Array(bytes);
    var hexBytes = [];

    for (var i = 0; i < bytes.length; ++i) {
        var byteString = bytes[i].toString(16);
        if (byteString.length < 2)
            byteString = "0" + byteString;
        hexBytes.push(byteString);
    }

    return hexBytes.join("");
}
