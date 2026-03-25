let channel;
onmessage = (event) => {
    if (event.data.channel) {
        channel = event.data.channel;
        let didOpen = false;
        channel.onopen = () => {
            if (!didOpen) {
                didOpen = true;
                self.postMessage("opened");
            }
        };
        channel.onerror = () => self.postMessage("errored");
        channel.onclose = () => self.postMessage("closed");
        channel.onmessage = event => self.postMessage(event.data);
        // The channel may already be open if the connection was established
        // while the channel was being transferred to this worker.
        if (channel.readyState === "open" && !didOpen) {
            didOpen = true;
            self.postMessage("opened");
        }
    }
    if (event.data.message) {
        if (channel)
            channel.send(event.data.message);
    }
    if (event.data.close) {
        if (channel)
            channel.close();
    }
};
self.postMessage("registered");
