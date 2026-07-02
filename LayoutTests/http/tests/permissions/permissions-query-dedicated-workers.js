onmessage = function(e) {
    navigator.permissions.query({ name: "geolocation" }).then((status) => {
      postMessage(status.state);
    });
}