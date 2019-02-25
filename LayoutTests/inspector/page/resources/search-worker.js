// Worker resource with the SEARCH-STRING.

// Worker resource with the OTHER-STRING.

self.addEventListener("message", (event) => {
    self.postMessage("echo: " + event.data);
});
