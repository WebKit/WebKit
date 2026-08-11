class MockRTCRtpTransformer {
    constructor(transformer) {
        this.askKeyFrame = false;
        this.context = transformer;
        this.context.options.port.onmessage = (event) => {
            if (event.data === "startKeyFrames")
                this.askKeyFrame = true;
            else if (event.data === "endKeyFrames")
                this.askKeyFrame = false;
        };
        this.start();
    }
    start()
    {
        this.reader = this.context.readable.getReader();
        this.writer = this.context.writable.getWriter();
        this.process();
        this.context.options.port.postMessage("started " + this.context.options.mediaType + " " + this.context.options.side);
    }

    process()
    {
        this.reader.read().then(chunk => {
            if (chunk.done)
                return;

            this.writer.write(chunk.value);

            if (this.context.options.mediaType === "video") {
                if (chunk.value instanceof RTCEncodedVideoFrame)
                    this.context.options.port.postMessage("video frame " + chunk.value.type);

                if (this.askKeyFrame) {
                    if (this.context.options.side === "sender")
                         this.context.generateKeyFrame();
                    else
                         this.context.sendKeyFrameRequest();
                }
            }

            this.process();
        });
    }
};

onrtctransform = (event) => {
    new MockRTCRtpTransformer(event.transformer);
};

self.postMessage("registered");
