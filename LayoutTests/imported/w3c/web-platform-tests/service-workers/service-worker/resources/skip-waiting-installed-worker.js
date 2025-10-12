var saw_activate_event = false;
var activate_event_resolvers = [];

self.addEventListener('activate', function() {
    saw_activate_event = true;
    while (activate_event_resolvers.length > 0) {
      activate_event_resolvers.shift()();
    }
  });

function waitForActivateEvent() {
  if (saw_activate_event) {
    return Promise.resolve();
  }
  
  return new Promise(function(resolve) {
    activate_event_resolvers.push(resolve);
  });
}

self.addEventListener('message', function(event) {
    var port = event.data.port;
    event.waitUntil(self.skipWaiting()
      .then(function(result) {
          if (result !== undefined) {
            port.postMessage('FAIL: Promise should be resolved with undefined');
            return;
          }
          return waitForActivateEvent();
        })
      .then(function() {
          if (!saw_activate_event) {
            port.postMessage(
                'FAIL: Promise should be resolved after activate event is dispatched');
            return;
          }

          if (self.registration.active.state !== 'activating') {
            port.postMessage(
                'FAIL: Promise should be resolved before ServiceWorker#state is set to activated');
            return;
          }

          port.postMessage('PASS');
        })
      .catch(function(e) {
          port.postMessage('FAIL: unexpected exception: ' + e);
        }));
  });
