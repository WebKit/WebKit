onactivate = (e) => e.waitUntil(clients.claim());

async function doTest(event)
{
    if (!event.data.startsWith("USERAGENT")) {
        event.source.postMessage("FAIL: received unexpected message from client");
        return;
    }

    if (event.data === "USERAGENT-NAVIGATOR") {
        event.source.postMessage(navigator.userAgent);
        return;
    }

    if (event.data === "USERAGENT-FETCH") {
        var response = await fetch("/fetch/api/resources/inspect-headers.py?headers=User-Agent")
        event.source.postMessage(response.headers.get("x-request-user-agent"));
        return;
    }

    event.source.postMessage("FAIL: Unknown test");
}


self.addEventListener("message", doTest);
