self.onmessage = (event) => {
  navigator.permissions.query({ name: "geolocation" }).then((status) => {
    event.source.postMessage(status.state);
  });
};
