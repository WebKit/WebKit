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


self.addEventListener("install", installTest);
self.addEventListener("activate", activateTest);
self.addEventListener("message", messageTest);
self.addEventListener("fetch", fetchTest);
