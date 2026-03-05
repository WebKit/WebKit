class MockRTCRtpTransformer {
    constructor(transformer) {
        this.delayWrite = false;
        this.context = transformer;
        this.context.options.port.onmessage = (event) => {
            if (event.data === "delayWrite")
                this.delayWrite = true;
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
        this.reader.read().then(async chunk => {
            if (chunk.done)
                return;

            const shouldDelay = this.delayWrite;
            if (shouldDelay) {
                this.context.options.port.postMessage("delaying");
                await new Promise(resolve => setTimeout(resolve, 500));
            }

            try {
                this.writer.write(chunk.value);
            } catch (e) {
            }

            if (shouldDelay)
                this.delayWrite = false;

            this.process();
        });
    }
};


onrtctransform = (event) => {
    new MockRTCRtpTransformer(event.transformer);
};

self.postMessage("registered");
