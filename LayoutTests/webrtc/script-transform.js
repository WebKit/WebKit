class MockRTCRtpTransformer extends RTCRtpScriptTransformer {
    constructor() { }
    start(readableStream, writableStream) { readableStream.pipeTo(writableStream); }
};

registerRTCRtpScriptTransformer("MockRTCRtpTransform", MockRTCRtpTransformer);
self.postMessage("started");
