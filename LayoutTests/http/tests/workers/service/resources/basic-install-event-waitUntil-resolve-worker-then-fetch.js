let resolvedExtendLifetimePromise = false;

self.addEventListener("install", (event) => {
    event.waitUntil(new Promise((resolve, reject) => {
        setTimeout(() => {
            resolvedExtendLifetimePromise = true;
            resolve();
        }, 50);
    }));
});

self.addEventListener("message", (event) => {
    fetch("http://localhost:8000/cookies/resources/setCookies.cgi", { credentials: "include", headers: { "X-SET-COOKIE": "firstPartyCookie=sw;Path=/" } }).then((r) => r.text()).then((text) => {
        event.source.postMessage(`setCookies response: ${text}`);
    });

});

