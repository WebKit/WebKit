onmessage = function() {
    fetch("/resources/square100.png?page-cache-worker-in-subframe-fetch-after-pagehide", { cache: "no-store" }).then(
        function() { postMessage("resolved"); },
        function() { postMessage("rejected"); });
};

postMessage("ready");
