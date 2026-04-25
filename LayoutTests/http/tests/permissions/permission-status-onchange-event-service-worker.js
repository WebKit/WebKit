var permissionStatus;

self.onmessage = (event) => {
  navigator.permissions.query({ name: "geolocation" }).then((result)=>{
    permissionStatus = result;

    permissionStatus.onchange = () => event.source.postMessage(permissionStatus.state);

    event.source.postMessage(permissionStatus.state);
  });
};
