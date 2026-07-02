navigator.serviceWorker.register("/WebKit/service-workers/resources/lengthy-pass.py?delay=0.5", { scope: "/WebKit/service-workers/resources/" });
setTimeout(() => self.postMessage("ok"), 100);
