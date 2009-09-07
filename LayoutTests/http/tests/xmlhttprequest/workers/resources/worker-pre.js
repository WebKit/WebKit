// Code to provide a consistent test interface for workers and shared workers.
if (self.postMessage) {
    if (self.init)
        init();
 } else {
    self.onconnect = function(event) {
        self.postMessage = function(message) { event.ports[0].postMessage(message); };
        event.ports[0].onmessage = function (evt) {
            if (self.onmessage)
                self.onmessage(evt);
        };
        if (self.init)
            init();
    };
 }
