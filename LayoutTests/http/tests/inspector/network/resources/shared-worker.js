onconnect = function(event)
{
    var port = event.ports[0];
    port.postMessage("Done.");
}

