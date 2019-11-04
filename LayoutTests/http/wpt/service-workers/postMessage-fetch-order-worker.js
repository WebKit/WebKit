var postMessageFetchEventsOrder = [];
async function doTest(event)
{
    if (event.data === "postMessageBeforeFetch") {
        postMessageFetchEventsOrder.push("postMessageBeforeFetch");
        return;
    }
    if (event.data === "postMessageAfterFetch") {
        postMessageFetchEventsOrder.push("postMessageAfterFetch");
        event.source.postMessage(postMessageFetchEventsOrder);
        postMessageFetchEventsOrder = [];
    }
}

self.addEventListener("message", doTest);

async function doFetch(event)
{
    if (event.request.url.includes("betweenPostMessages"))
        postMessageFetchEventsOrder.push("fetch");
    event.respondWith(new Response("Intercepted"));
}

self.addEventListener("fetch", doFetch);
