let subworkerForURL = new Map;

self.addEventListener("message", (event) => {
    let {url, data} = event.data;
    if (!url)
        return;

    let subworker = subworkerForURL.get(url);
    if (!subworker) {
        subworker = new Worker(url);
        subworkerForURL.set(url, subworker);

        subworker.addEventListener("message", (event) => {
            self.postMessage({
                url,
                data: event.data,
            });
        });
    }

    if (data)
        subworker.postMessage(data);
    else if (data === undefined) {
        subworker.terminate();
        subworkerForURL.delete(url);
    }
});
