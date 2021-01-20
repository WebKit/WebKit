var counter = 0;
onmessage = function(e) {
    if (e.data !== "start")
        return;

    var promise1 = fetch("/misc/resources/delayed-log.php?delay=100").then(() => {
        postMessage("FAIL: promise 1 resolved");
    }, () => {
        postMessage("FAIL: promise 1 rejected");
    });

    var stream = new ReadableStream({"start": function(controller) {
        controller.enqueue(new Uint8Array(1));
        // Not closing the stream so that below promise will not fulfill.
    }});
    var promise2 = new Response(stream).text().then(() => {
        postMessage("FAIL: promise 2 resolved");
    }, () => {
        postMessage("FAIL: promise 2 rejected");
    });

    var promise3 = new Request("test", {method: "POST", body: new Blob(["ab"])}).text().then(() => {
        // Not posting message as it could make the test flaky since we are not sure the test will end before the blob load finishes.
    }, () => {
        postMessage("FAIL: promise 3 rejected");
    });

    postMessage(counter++);
}
