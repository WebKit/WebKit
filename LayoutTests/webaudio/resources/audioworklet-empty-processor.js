class EmptyProcessor extends AudioWorkletProcessor {
    process()
    {
        return true;
    }
}

registerProcessor("empty", EmptyProcessor);
