onrtctransform = (event) => {
    const transformer = event.transformer;
    transformer.port.onmessage = (event) => transformer.port.postMessage(event.data);

    self.postMessage("started");

    transformer.reader = transformer.readable.getReader();
    transformer.writer = transformer.writable.getWriter();
    function process(transformer)
    {
        transformer.reader.read().then(chunk => {
            if (chunk.done)
                return;

            transformer.writer.write(chunk.value);
            transformer.writer.write(chunk.value);
            process(transformer);
        });
    }

    process(transformer);
};
self.postMessage("registered");
