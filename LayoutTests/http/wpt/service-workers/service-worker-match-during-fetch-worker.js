function doFetch(e)
{
    let resultsPromise;
    if (self.location.href.includes("get")) {
        resultsPromise = self.clients.get(e.resultingClientId).then(result => {
            let text = "<html><body><script>window.result = '";
            if (result)
                text += result.url;
            else
                text += "none";
            text += "';</script></body></html>";
            return new Response(text, {  headers: [["Content-Type", "text/html; charset=utf-8"]] });
        });
    } else if (self.location.href.includes("matchall")) {
        resultsPromise = self.clients.matchAll().then(results => {
            let text = "<html><body><script>window.result = '";
            if (!results.length)
                text += "none";
            else
                results.forEach(result => text += result.url + ",");
            text += "';</script></body></html>";
            return new Response(text, {  headers: [["Content-Type", "text/html; charset=utf-8"]] });
        });
    } else if (self.location.href.includes("postmessage")) {
       resultsPromise = Promise.any([
           self.clients.get(e.resultingClientId).then(client => {
               if (client)
                   client.postMessage("got client " + client.url);
           }),
           new Promise(resolve => setTimeout(resolve, 1000))
       ]);
       resultsPromise = resultsPromise.then(() => {
           const text = "<html><body><script>window.result = ''; navigator.serviceWorker.onmessage = e => window.result += e.data;</script></body></html>";
           return new Response(text, {  headers: [["Content-Type", "text/html; charset=utf-8"]] });
       });
       setInterval(async () => {
           const client = await self.clients.get(e.resultingClientId);
           if (client)
               client.postMessage("post fetch");
       }, 100);
    }

    e.respondWith(resultsPromise);
    return resultsPromise;
}

self.addEventListener("fetch", e => e.waitUntil(doFetch(e)));
