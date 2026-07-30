let waitingToRead = null;

addEventListener("fetch", (event) => {
    const url = new URL(event.request.url);

    if (url.searchParams.get("release") === "1") {
        if (waitingToRead) {
            waitingToRead.resolve();
            waitingToRead = null;
        }
        event.respondWith(new Response("released"));
        return;
    }

    if (!url.pathname.endsWith("backpressure-endpoint"))
        return;

    event.respondWith((async () => {
        await new Promise((resolve) => { waitingToRead = { resolve }; });

        const reader = event.request.body.getReader();
        let total = 0;
        while (true) {
            const { value, done } = await reader.read();
            if (done)
                break;
            total += value.byteLength;
        }
        return new Response(String(total), {
            headers: { "Content-Type": "text/plain" },
        });
    })());
});
