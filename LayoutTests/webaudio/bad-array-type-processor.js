Array.prototype[1] = [new Float32Array(0x80)];

let count = 0;
class CustomProcessor extends AudioWorkletProcessor {
    process(inputs, outputs, parameters) {
        count++;

        if (count === 2) {
            Array.prototype[1] = 0x1234;
        }

        return true;
    }
}

let processor = null;
registerProcessor('bad-array-type-processor', function (options) {
    if (processor !== null)
        return processor;

    return processor = new CustomProcessor(options);
});
