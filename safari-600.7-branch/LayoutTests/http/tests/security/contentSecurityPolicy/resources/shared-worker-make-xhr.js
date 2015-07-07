onconnect = function (event) {
    var port = event.ports[0];
    try {
        var xhr = new XMLHttpRequest;
        xhr.open("GET", "http://127.0.0.1:8000/xmlhttprequest/resources/get.txt", true); 
        port.postMessage("xhr allowed");
    } catch(e) {
        port.postMessage("xhr blocked");
    }
};
