var workerId = location.search.substring(4);
postMessage("Worker " + workerId + " started.");
doWork();
setInterval(doWork, 1000);
function doWork() {
}

