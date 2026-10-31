onrtctransform = event => {
    const transformer = event.transformer;
    const port = transformer.options.port;
    let fired = false;
    transformer.reader = transformer.readable.getReader();
    transformer.writer = transformer.writable.getWriter();
    function process() {
        transformer.reader.read().then(({ value, done }) => {
            if (done)
                return;
            if (!fired) {
                fired = true;
                try {
                    // 0x80000000 bytes truncates to a negative value when narrowed to `int`.
                    value.data = new ArrayBuffer(0x80000000);
                    transformer.writer.write(value);
                    port.postMessage("sent");
                } catch (e) {
                    // The bug is unreachable on hosts where the allocation fails.
                    transformer.writer.write(value);
                    port.postMessage("skipped");
                }
            } else
                transformer.writer.write(value);
            process();
        });
    }
    process();
};
self.postMessage("registered");
