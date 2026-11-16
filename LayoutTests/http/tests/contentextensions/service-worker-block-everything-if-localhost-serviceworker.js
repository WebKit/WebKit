onmessage = async e => {
    await self.clients.claim();
    e.source.postMessage("done");
}

onfetch = e => {
   e.respondWith(fetch(e.request));
};
