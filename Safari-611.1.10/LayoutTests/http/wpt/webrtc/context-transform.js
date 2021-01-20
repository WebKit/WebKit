class MockRTCRtpTransformer extends RTCRtpScriptTransformer {
    constructor() {
        super();
        this.askKeyFrame = false;
        this.port.onmessage = (event) => {
            if (event.data === "startKeyFrames")
                this.askKeyFrame = true;
            else if (event.data === "endKeyFrames")
                this.askKeyFrame = false;
        };
    }
    start(readableStream, writableStream, context)
    {
        this.reader = readableStream.getReader();
        this.writer = writableStream.getWriter();
        this.process();
        this.context = context;
        this.port.postMessage("started " + context.mediaType + " " + context.side);
    }

    process()
    {
        this.reader.read().then(chunk => {
            if (chunk.done)
                return;

            this.writer.write(chunk.value);

            if (this.context.mediaType === "video") {
                if (chunk.value instanceof RTCEncodedVideoFrame)
                    this.port.postMessage("video frame " + chunk.value.type);

                if (this.askKeyFrame)
                    this.context.requestKeyFrame();
            }

            this.process();
        });
    }
};

registerRTCRtpScriptTransformer("MockRTCRtpTransform", MockRTCRtpTransformer);
self.postMessage("registered");
