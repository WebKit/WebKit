oninstall = e => {
    e.addRoutes([{
        condition: {
            urlPattern: new URLPattern({pathname: '*'}),
        },
        source: 'network'
    }]);
}

var fetchCount = 0;
onfetch = () => {
    fetchCount++;
}

onmessage = e => {
    e.source.postMessage(!fetchCount ? "OK" : "KO: " + fetchCount);
}
