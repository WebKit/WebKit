
function requestUsage() {
    port = self.port || self;
    function errorCallback(error)
    {
        port.postMessage("errorCallback called");
    }

    function usageCallback(usage, quota)
    {
        port.postMessage("result: " + JSON.stringify({ usage: usage, quota: quota}));
    }

    port.postMessage("Requesting quota from " + navigator.webkitTemporaryStorage);
    navigator.webkitTemporaryStorage.queryUsageAndQuota(usageCallback, errorCallback);
    return true;
}
