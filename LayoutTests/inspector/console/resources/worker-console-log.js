console.log("WORKER STARTED");

self.addEventListener("message", (event) => {
    // FIXME: <https://webkit.org/b/154213> Web Inspector: Improve support for console logging within workers
    // Currently the arguments to the console.log are lost when crossing thread boundaries. This should be
    // addressed when adding better worker debugging in general.
    console.log("WORKER RECEIVED MESSAGE", "argument");
});

