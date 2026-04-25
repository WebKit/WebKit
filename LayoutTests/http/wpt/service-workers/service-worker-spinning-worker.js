let pendingEvent = null;

function respondToPendingEvent()
{
    if (!pendingEvent)
        return;

    pendingEvent.ports[0].postMessage('received');
    pendingEvent = null;
}

if (self.registration.scope.includes("spin-run"))
    while(true) { };

function installTest(event)
{
    if (self.registration.scope.includes("spin-install"))
        while(true) { };
    if (self.registration.scope.includes("spin-after-install"))
        self.setTimeout(() => { while(true) { }; }, 0);
}

function activateTest(event)
{
    if (self.registration.scope.includes("spin-activate"))
        while(true) { };
    if (self.registration.scope.includes("spin-after-activate"))
        self.setTimeout(() => { while(true) { }; }, 0);
}

function messageTest(event)
{
    switch (event.data) {
    case "push":
        self.internals.schedulePushEvent("test");
        pendingEvent = event;
        return;
    case "pushsubscriptionchange":
        self.internals.schedulePushSubscriptionChangeEvent(null, null);
        pendingEvent = event;
        return;
    }

    if (self.registration.scope.includes("spin-message"))
        while(true) { };
    if (self.registration.scope.includes("spin-after-message"))
        self.setTimeout(() => { while(true) { }; }, 0);
}

function fetchTest(event)
{
    if (self.registration.scope.includes("spin-fetch"))
        while(true) { };
    if (self.registration.scope.includes("spin-after-fetch"))
        self.setTimeout(() => { while(true) { }; }, 0);
    event.respondWith(new Response("ok"));
}

function pushTest(event)
{
    respondToPendingEvent();

    if (self.registration.scope.includes("spin-push"))
        while(true) { };
    if (self.registration.scope.includes("spin-after-push"))
        self.setTimeout(() => { while(true) { }; }, 0);
}

function pushSubscriptionChangeTest(event)
{
    respondToPendingEvent();

    if (self.registration.scope.includes("spin-pushsubscriptionchange"))
        while(true) { };
    if (self.registration.scope.includes("spin-after-pushsubscriptionchange"))
        self.setTimeout(() => { while(true) { }; }, 0);
}

self.addEventListener("install", installTest);
self.addEventListener("activate", activateTest);
self.addEventListener("message", messageTest);
self.addEventListener("fetch", fetchTest);
self.addEventListener("push", pushTest);
self.addEventListener("pushsubscriptionchange", pushSubscriptionChangeTest);
