// Worker resource with the SEARCH-STRING.

self.addEventListener("message", (event) => {
    self.postMessage("echo: " + event.data);
});
