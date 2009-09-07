// Send "Foo" back to the parent page - script works for both shared and dedicated workers
if (self.postMessage)
    postMessage("Foo");
else
    self.onconnect = function(event) { event.ports[0].postMessage("Foo"); }

