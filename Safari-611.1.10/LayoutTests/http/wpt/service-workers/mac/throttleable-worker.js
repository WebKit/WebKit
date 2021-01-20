onactivate = (e) => e.waitUntil(clients.claim());

function checkThrottleable(event)
{
    event.source.postMessage(self.internals ? self.internals.isThrottleable : "needs internals");
}

self.addEventListener("message", checkThrottleable);
