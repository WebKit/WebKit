var req = new Request("thisisatest");
postMessage("starting");
req.text().then(() => {
    postMessage("finished");
})
postMessage("started");
