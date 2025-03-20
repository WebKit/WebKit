self.addEventListener("install", () => {
    console.log("Service Worker installing...");
    self.skipWaiting(); // Optional: activates the SW immediately
});

self.addEventListener("activate", () => {
    console.log("Service Worker activated!");
});

self.addEventListener("message", async (e) => {
    console.log("Received message in Service Worker:", e.data);

    if (typeof e.data === 'number') {
        const response = await fetch(`../generate-byte.py?size=${e.data}`);
        const blob = await response.blob();
        console.log("Fetched blob in Service Worker:", e.data);

        e.source.postMessage(blob);
    } else {
        e.source.postMessage(false);
    }
});
