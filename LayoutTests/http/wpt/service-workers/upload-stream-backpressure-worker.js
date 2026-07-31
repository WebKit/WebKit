let waitingToStart = null;
let waitingToContinue = null;

addEventListener("fetch", (event) => {
    const url = new URL(event.request.url);
    const releaseStep = url.searchParams.get("release");

    if (releaseStep === "1") {
        if (waitingToStart) {
            waitingToStart.resolve();
            waitingToStart = null;
        }
        event.respondWith(new Response("released"));
        return;
    }
    if (releaseStep === "2") {
        if (waitingToContinue) {
            waitingToContinue.resolve();
            waitingToContinue = null;
        }
        event.respondWith(new Response("released"));
        return;
    }

    if (!url.pathname.endsWith("backpressure-endpoint"))
        return;

    event.respondWith((async () => {
        await new Promise((resolve) => { waitingToStart = { resolve }; });

        const reader = event.request.body.getReader();
        let total = 0;

        // Consume a bounded number of chunks then pause to prove that backpressure
        // re-engages once the service worker stops reading.
        const CHUNKS_BEFORE_PAUSE = 4;
        for (let i = 0; i < CHUNKS_BEFORE_PAUSE; ++i) {
            const { value, done } = await reader.read();
            if (done)
                return new Response(String(total), { headers: { "Content-Type": "text/plain" } });
            total += value.byteLength;
        }

        await new Promise((resolve) => { waitingToContinue = { resolve }; });

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
