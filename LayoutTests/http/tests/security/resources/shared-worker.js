self.addEventListener('connect', function(event) {
	var port = event.ports[0];
	port.addEventListener('message', function(event) {
		port.postMessage('Connected successfully');
	}, false);
	port.start();
}, false);
