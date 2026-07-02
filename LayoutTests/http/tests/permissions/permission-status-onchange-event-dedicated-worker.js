var permissionStatus;

onmessage = function(e) {
  navigator.permissions.query({ name: "geolocation" }).then((result)=>{
    permissionStatus = result;

    permissionStatus.onchange = () => postMessage(permissionStatus.state);

    postMessage(permissionStatus.state);
  });
}
