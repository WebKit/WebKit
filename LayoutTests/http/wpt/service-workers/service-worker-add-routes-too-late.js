let installEvent;
oninstall = e => {
   installEvent = e;
}

onmessage = async e => {
    let result = "KO";
    try {
        installEvent.addRoutes([{
            condition: {urlPattern: new URLPattern({pathname: '/**/direct.txt'})},
            source: 'network'
        }]);
    } catch (e) {
       result = "OK";
    }
    e.source.postMessage(result);
}
