async function doClaim()
{
    await clients.claim();
    setTimeout(doClaim, 50);
}

onactivate = (e) => {
    e.waitUntil(doClaim());
};

let state;
let loads = [];
onfetch = async (e) => {
    loads.push({url: e.request.url, cache: e.request.cache});
}

onmessage = (event) => {
    if (event.data === "getState") {
        event.source.postMessage(state);
        return;
    }

    if (event.data === "startTest") {
        state = "startTest";
        loads = [];
        return;
    }

    if (event.data === "endTest") {
        state = "endTest";
        event.source.postMessage(loads);
        return;
    }

    doClaim();
}
