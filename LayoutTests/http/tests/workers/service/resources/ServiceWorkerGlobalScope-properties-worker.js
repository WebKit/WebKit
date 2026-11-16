self.addEventListener("message", (event) => {
    let result = eval(event.data);
    event.source.postMessage(event.data + " returned " + JSON.stringify(result));
});
