self.addEventListener("message", (event) => {
    fetch("/resources/dummy.js").then(() => {
        event.source.postMessage("FAIL - should have blocked dummy.js");
    }).catch(() => {
        fetch("/resources/dummy.css").then(() => {
            event.source.postMessage("PASS - blocked dummy.js, allowed dummy.css");
        }).catch(() => {
            event.source.postMessage("FAIL - should have allowed dummy.css");
        });
    });
});
