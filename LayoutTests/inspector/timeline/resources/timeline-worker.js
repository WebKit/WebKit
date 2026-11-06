function workerTimerFired() {
    return 42;
}

self.addEventListener("message", (event) => {
    switch (event.data) {
    case "fire-timer":
        setTimeout(workerTimerFired);
        break;
    case "profile":
        console.profile("worker");
        self.postMessage("profile");
        break;
    case "profile-end":
        console.profileEnd("worker");
        self.postMessage("profile-end");
        break;
    case "busy":
        self.postMessage("busy");

        // Intentional infinite loop, so that the Worker never handles another inspector message.
        while (true);
    }
});
