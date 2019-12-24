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

async function waitForServiceWorkerNoLongerRunning(worker)
{
    if (!window.internals)
        return Promise.reject("requires internals");

    let count = 100;
    while (--count > 0 && await internals.isServiceWorkerRunning(worker)) {
        worker.postMessage("test");
        await new Promise(resolve => setTimeout(resolve, 50));
    }
    if (count === 0)
        return Promise.reject("service worker is still running");
}
