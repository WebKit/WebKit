onconnect = async (e) => { 
    const  port = e.ports[0];
    port.postMessage({ isControlled : !!navigator.serviceWorker.controller });
    port.onmessage = (event) => {
        navigator.serviceWorker.controller.postMessage(event.data);
        navigator.serviceWorker.onmessage = (event) => port.postMessage(event.data);
    };
}
