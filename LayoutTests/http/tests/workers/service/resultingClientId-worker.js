onmessage = async e => {
    if (e.data === "getClientId") {
        e.source.postMessage(self.clientId);
        return;
    }
    if (e.data === "pingResultingClientId") {
        const client = await self.clients.get(self.resultingClientId);
        if (client)
            client.postMessage("pong");
        return;
    }
    e.source.postMessage(self.resultingClientId);
};

onfetch = async e => {
    self.clientId = e.clientId;
    if (e.resultingClientId)
        self.resultingClientId = e.resultingClientId;

    const text = `<html><body><script>
        onload = async () => {
            navigator.serviceWorker.controller.postMessage("pingResultingClientId");
            const pong = await new Promise(resolve => navigator.serviceWorker.onmessage = e => resolve(e.data));
            if (pong != "pong") {
                console.log("did not get pong but " + pong);
                return;
            }
            await fetch("/");
            history.back();
        };
        </script></body></html>`;
    const headers = [
       ["Cache-Control", "no-cache, no-store, must-revalidate, max-age=0"],
       ["Content-Type", "text/html"],
    ];
    if (e.request.url.includes("with-coop"))
        headers.push(["Cross-Origin-Opener-Policy", "same-origin"]);

    e.respondWith(new Response(text, { headers }));
}
