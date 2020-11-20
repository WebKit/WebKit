class MockRTCRtpTransformer extends RTCRtpScriptTransformer {
    constructor() {
        super();
        this.port.onmessage = (event) => this.port.postMessage(event.data);
    }
    start(readableStream, writableStream)
    {
        self.postMessage("started");
        this.reader = readableStream.getReader();
        this.writer = writableStream.getWriter();
        this.process();
    }

    process()
    {
        this.reader.read().then(chunk => {
            if (chunk.done)
                return;
            if (chunk.value instanceof RTCEncodedVideoFrame)
                self.postMessage("video chunk");
            else if (chunk.value instanceof RTCEncodedAudioFrame)
                self.postMessage("audio chunk");
            this.writer.write(chunk.value);
            this.process();
        });
    }
};

registerRTCRtpScriptTransformer("MockRTCRtpTransform", MockRTCRtpTransformer);
self.postMessage("registered");
