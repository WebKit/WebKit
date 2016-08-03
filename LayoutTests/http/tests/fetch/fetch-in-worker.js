var counter = 0;
onmessage = function(e) {
    if (e.data !== "start")
        return;

    var promise1 = fetch("/misc/resources/delayed-log.php?delay=100").then(() => {
        postMessage("FAIL: promise 1 resolved");
    }, () => {
        postMessage("FAIL: promise 1 rejected");
    });

    var promise2 = new Request("test", {method: "POST", body: new Blob(["ab"])}).text().then(() => {
        postMessage("FAIL: promise 2 resolved");
    }, () => {
        postMessage("FAIL: promise 2 rejected");
    });

    postMessage(counter++);
}
