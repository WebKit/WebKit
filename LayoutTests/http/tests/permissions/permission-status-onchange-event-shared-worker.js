var permissionStatus;

onconnect = function (e) {
  var port = e.ports[0];

  port.onmessage = function(e) {
    navigator.permissions.query({ name: "geolocation" }).then((result)=>{
      permissionStatus = result;

      permissionStatus.onchange = () => port.postMessage(permissionStatus.state);

      port.postMessage(permissionStatus.state);
    });
  }

}
