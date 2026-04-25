let installEvent;
oninstall = e => {
   installEvent = e;
}

onmessage = async e => {
    let result = "KO";
    try {
        await installEvent.addRoutes([{
            condition: {urlPattern: new URLPattern({pathname: 'direct.txt', port: 80})},
            source: 'network'
        }]);
    } catch (e) {
       result = "OK";
    }
    e.source.postMessage(result);
}
