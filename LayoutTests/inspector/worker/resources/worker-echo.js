self.addEventListener("message", (event) => {
    self.postMessage({
        echo: true,
        data: event.data,
    });
});
