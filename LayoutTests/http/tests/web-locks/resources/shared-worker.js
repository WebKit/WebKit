self.onconnect = (event) => {
    navigator.locks.request('abc', {mode: 'shared'}, () => {
        event.ports[0].postMessage('PASS');
    }).catch((error) => event.ports[0].postMessage(`FAIL - ${error}`));
}
