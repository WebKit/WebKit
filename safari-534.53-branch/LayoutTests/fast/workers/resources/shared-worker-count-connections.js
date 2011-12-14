var ports = [];
onconnect = function(event) {
  ports.push(event.ports[0]);
  for (var i = 0 ; i < ports.length ; i++) {
    ports[i].postMessage(ports.length);
  }
};
