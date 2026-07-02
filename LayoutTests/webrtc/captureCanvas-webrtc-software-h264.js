function printRectangle(canvas)
{
    var context = canvas.getContext("2d");
    context.fillStyle = canvas.color;
    context.fillRect(0, 0, 100, 100);
    setTimeout(() => printRectangle(canvas), 50);
}

function testCanvas(testName, canvas, isSame, count)
{
    var array1 = canvas.getContext("2d").getImageData(20, 20, 60, 60).data;
    if (count === undefined)
        count = 0;
    canvas2.getContext("2d").drawImage(video, 0 ,0);
    array2 = canvas2.getContext("2d").getImageData(20, 20, 60, 60).data;
    var isEqual = true;
    for (var index = 0; index < array1.length; ++index) {
        // Rough comparison since we are compressing data.
        // This test still catches errors since we are going from green to blue to red.
        if (Math.abs(array1[index] - array2[index]) > 100) {
            isEqual = false;
            continue;
        }
    }
    if (isEqual === isSame)
        return;

    if (count === 200)
        return Promise.reject(testName + " failed: tried " + count + " to get " + isSame + " but got " + isEqual);

    return waitFor(50).then(() => {
        return testCanvas(testName, canvas, isSame, ++count);
    });
}

var canvas0Track;
var sender;
function doTest(test, setupOffer)
{
    canvas0.color = "green";
    printRectangle(canvas0);
    const shouldMatchCanvas1AfterReplaceTrack = true;
    return new Promise((resolve, reject) => {
        createConnections((firstConnection) => {
            var stream = canvas0.captureStream();
            canvas0Track = stream.getVideoTracks()[0];
            sender = firstConnection.addTrack(canvas0Track, stream);
        }, (secondConnection) => {
            secondConnection.ontrack = (trackEvent) => {
                resolve(trackEvent.streams[0]);
            };
        }, { observeOffer : (offer) => {
            offer.sdp = setupOffer(offer.sdp);
            return offer;
        }
        });
        setTimeout(() => reject("Test timed out"), 5000);
    }).then((stream) => {
        video.srcObject = stream;
        return video.play();
    }).then(() => {
        return testCanvas("test1", canvas0, true);
    }).then(() => {
        canvas1.color = "blue";
        printRectangle(canvas1);
        var stream = canvas1.captureStream();
        return sender.replaceTrack(stream.getVideoTracks()[0]);
    }).then(() => {
        return waitFor(200);
    }).then(() => {
        return testCanvas("test2", canvas1, shouldMatchCanvas1AfterReplaceTrack);
    }).then(() => {
        return testCanvas("test3", canvas0, !shouldMatchCanvas1AfterReplaceTrack);
    }).then(() => {
        return sender.replaceTrack(canvas0Track);
    }).then(() => {
        canvas0.color = "red";
        // Let's wait for red color to be printed on canvas0
        return waitFor(200);
    }).then(() => {
        return testCanvas("test4", canvas0, true);
    });
}
