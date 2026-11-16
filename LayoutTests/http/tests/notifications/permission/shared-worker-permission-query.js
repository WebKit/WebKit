onconnect = function (e) {
  var port = e.ports[0];

  port.onmessage = function (e) {
    navigator.permissions.query({ name: "notifications" }).then((status) => {
      port.postMessage(status.state);
    });
  };
}
