self.onmessage = async (event) => {
    const status = await navigator.permissions.query({ name: "geolocation" });
    event.source.postMessage(status.state);
};
