self.addEventListener("message", (event) => {
    if (event.data === "DONE") {
        event.source.postMessage(event.data);
        return;
    }
    let result = eval(event.data);
    event.source.postMessage(event.data + " returned " + JSON.stringify(result));
});
