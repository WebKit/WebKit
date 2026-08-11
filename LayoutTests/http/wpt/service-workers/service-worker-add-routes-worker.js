var installEvent;
var resolveInstallEvent;
oninstall = e => {
   installEvent = e;
   installEvent.waitUntil(new Promise(resolve => resolveInstallEvent = resolve));
}

onmessage = e => {
  if (e.data == "finish") {
    resolveInstallEvent();
    return;
  }
  try {
    const routes = [];
    for (let i = 0; i < e.data; i++) {
        routes.push({
            condition: {
                urlPattern: new URLPattern({pathname: 'direct.txt' + i}),
                get _or() {  throw "_or should not be used"; }
            },
            source: 'network'
        });
    }
    const promise = installEvent.addRoutes(routes);
    installEvent.waitUntil(promise);
    promise.then(() => e.source.postMessage("OK"), (error) => e.source.postMessage(error.toString()));
  } catch(exception) {
    e.source.postMessage(exception);
  }
}
