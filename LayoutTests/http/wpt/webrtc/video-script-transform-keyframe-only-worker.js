class MockRTCRtpTransformer {
    constructor(transformer) {
        this.context = transformer;
        this.start();
    }
    start()
    {
        this.reader = this.context.readable.getReader();
        this.writer = this.context.writable.getWriter();
        this.process();
    }

    process()
    {
        this.reader.read().then(chunk => {
            if (chunk.done)
                return;

            if (chunk.value.type === "delta")
                chunk.value = new ArrayBuffer();

            this.writer.write(chunk.value);
            this.process();
        });
    }
};

onrtctransform = (event) => {
    new MockRTCRtpTransformer(event.transformer);
};

self.postMessage("registered");
