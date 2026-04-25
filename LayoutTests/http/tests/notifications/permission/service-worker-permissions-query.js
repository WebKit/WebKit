self.onmessage = (event) => {
  navigator.permissions.query({ name: "notifications" }).then((status) => {
    event.source.postMessage(status.state);
  });
};
