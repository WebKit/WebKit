onmessage = function(e) {
    navigator.permissions.query({ name: "notifications" }).then((status) => {
      postMessage(status.state);
    });
}