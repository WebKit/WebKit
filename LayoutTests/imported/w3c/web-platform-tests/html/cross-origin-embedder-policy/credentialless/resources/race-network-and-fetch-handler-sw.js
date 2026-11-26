self.addEventListener("install", event => {
  event.waitUntil(event.addRoutes({
    condition: { requestMode: "no-cors" },
    source: "race-network-and-fetch-handler"
  }));
});

// Falling back leaves no-cors requests to the network side of the race.
self.addEventListener("fetch", () => {});
