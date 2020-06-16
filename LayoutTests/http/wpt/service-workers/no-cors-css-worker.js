let fetches = [];

async function doTest(event)
{
    if (event.data === "get-fetches") {
        event.source.postMessage(JSON.stringify(fetches));
        fetches = [];
        return;
    }
    event.source.postMessage("KO");
}

async function doFetch(event)
{
    if (event.request.destination === "document") {
        const link = event.request.url.substring(event.request.url.indexOf('?') + 1);
        const style = 'font: 12px "ahem"';
        event.respondWith(new Response("<!DOCTYPE html><html><header><link href='" + link + "' rel='stylesheet' type='text/css'></header><body><div style='" + style + "'>test</div></body></html>",
            { status: 200, headers: [["Content-Type", "text/html"]] }
        ));
        return;
    }
    fetches.push(event.request.url);
}

self.addEventListener("message", doTest);
self.addEventListener("fetch", doFetch);
