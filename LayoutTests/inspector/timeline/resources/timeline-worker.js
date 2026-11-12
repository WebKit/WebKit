function workerTimerFired() {
    return 42;
}

self.addEventListener("message", (event) => {
    if (event.data.block) {
        // A long task in a timer, which is recorded when it ends.
        setTimeout(() => {
            self.postMessage("blocked");

            // Handles no inspector messages until the page notifies, or after 30 seconds if it never does.
            Atomics.wait(new Int32Array(event.data.block), 0, 0, 30000);
        });
        return;
    }

    switch (event.data) {
    case "fire-timer":
        setTimeout(workerTimerFired);
        break;
    case "profile":
        console.profile("worker");
        break;
    case "busy":
        self.postMessage("busy");

        // Intentional infinite loop, so that the Worker never handles another inspector message.
        while (true);
    }
});
