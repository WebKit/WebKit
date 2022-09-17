onconnect = function (e) {
  var port = e.ports[0];

  port.onmessage = function (e) {
    navigator.permissions.query({ name: "geolocation" }).then((status) => {
      port.postMessage(status.state);
    });
  };
}
