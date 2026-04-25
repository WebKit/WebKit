async function waitForVideoSize(width, height)
{
    const max = 200
    let counter = 0;
    while (++counter < max && video.videoWidth != width && video.videoHeight != height)
        await waitFor(50);

    if (counter === max)
        return Promise.reject("Video size not expected : " + video.videoWidth + " " + video.videoHeight);
}

let pc1, pc2;
let localVideoTrack;

function testProfile(setProfileCallback, testName)
{
    promise_test(async (test) => {
        const localStream = await navigator.mediaDevices.getUserMedia({ video: true });
        localVideoTrack = localStream.getVideoTracks()[0];
        const stream = await new Promise((resolve, reject) => {
            createConnections((firstConnection) => {
                pc1 = firstConnection;
                firstConnection.addTrack(localVideoTrack, localStream);
            }, (secondConnection) => {
                pc2 = secondConnection;
                secondConnection.ontrack = (trackEvent) => {
                    resolve(trackEvent.streams[0]);
                };
            }, { observeOffer : (offer) => {
                offer.sdp = setProfileCallback(offer.sdp);
                return offer;
            }
            });
            setTimeout(() => reject("Test timed out"), 5000);
        });

        video.srcObject = stream;
        await video.play();
    }, testName);
}

function testResolutions(resolutions)
{
    resolutions.forEach(resolution => {
        promise_test(async (test) => {
            await localVideoTrack.applyConstraints({ width : resolution[0], height : resolution[1] });
            return waitForVideoSize(resolution[0], resolution[1]);
        }, "Video resolution test: " + resolution[0] + " " + resolution[1]);
    });

    resolutions.forEach(resolution => {
        promise_test(async (test) => {
            const parameters = pc1.getSenders()[0].getParameters();
            parameters.encodings[0].maxBitrate = 100000;
            pc1.getSenders()[0].setParameters(parameters);

            await localVideoTrack.applyConstraints({ width : resolution[0], height : resolution[1] });
            return waitForVideoSize(resolution[0], resolution[1]);
        }, "Video resolution test with maxBitrate: " + resolution[0] + " " + resolution[1]);
    });
}
