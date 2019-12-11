var remoteUrl = 'https://127.0.0.1:9443/WebKit/service-workers/resources/lengthy-pass.py?';
var counter = 0;
self.addEventListener('fetch', (event) => {
    event.respondWith(fetch(remoteUrl + counter++).then(response => new Response("PASS")).catch(e => new Response("Load failed: " + e)));
});
