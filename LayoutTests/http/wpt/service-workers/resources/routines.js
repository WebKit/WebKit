function waitFor(duration)
{
    return new Promise((resolve) => setTimeout(resolve, duration));
}

function withIframe(url) {
  return new Promise(function(resolve) {
      var frame = document.createElement('iframe');
      frame.src = url;
      frame.onload = function() { resolve(frame); };
      document.body.appendChild(frame);
    });
}

function waitForState(worker, state)
{
    if (!worker || worker.state == undefined)
        return Promise.reject(new Error('waitForState must be passed a ServiceWorker'));

    if (worker.state === state)
        return Promise.resolve(state);

    return new Promise(function(resolve, reject) {
      worker.addEventListener('statechange', function() {
          if (worker.state === state)
            resolve(state);
        });
        setTimeout(() => reject("waitForState timed out, worker state is " + worker.state), 5000);
    });
}

async function sendSyncMessage(worker, messageName, timeout)
{
    if (!window.internals)
        return Promise.reject("requires internals");

    const channel = new MessageChannel();
    const receivedMessage = new Promise(resolve => channel.port1.onmessage = resolve);
    const timedOut = new Promise((resolve, reject) => setTimeout(reject, timeout || 5000));
    worker.postMessage(messageName, [channel.port2]);
    return Promise.race([receivedMessage, timedOut]);
}

async function waitForServiceWorkerNoLongerRunning(worker)
{
    if (!window.internals)
        return Promise.reject("requires internals");

    const promise = internals.whenServiceWorkerIsTerminated(worker);
    let timer = setInterval(() => worker.postMessage("test"), 50);
    await promise;
    clearInterval(timer);
}

async function setupActivatedButNotRunningServiceWorker(workerScript, scope, whenActivated)
{
    if (window.testRunner) {
        testRunner.setUseSeparateServiceWorkerProcess(true);
        await fetch("").then(() => { }, () => { });
    }

    const registration = await navigator.serviceWorker.register(workerScript, { scope });
    await waitForState(registration.installing ? registration.installing : registration.active, "activated");

    if (whenActivated)
        await whenActivated(registration);

    if (window.testRunner) {
        testRunner.terminateServiceWorkers();
        await fetch("").then(() => { }, () => { });
    }
}
