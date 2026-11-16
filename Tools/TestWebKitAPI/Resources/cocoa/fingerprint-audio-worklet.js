
class FingerprintGenerator extends AudioWorkletProcessor {
    constructor() {
        super();
    }

    process(inputList, outputList) {
        let fingerprint = 0;
        for (let listIndex = 0; listIndex < Math.min(inputList.length, outputList.length); listIndex++) {
            let input = inputList[listIndex];
            let output = outputList[listIndex];
            for (let channel = 0; channel < Math.min(input.length, output.length); channel++) {
                let sampleCount = input[channel].length;
                for (let i = 0; i < sampleCount; i++) {
                    fingerprint += input[channel][i];
                    output[channel][i] = 0;
                }
            }
        }
        this.port.postMessage({ fingerprint });
        return true;
    }
}

registerProcessor("fingerprint-processor", FingerprintGenerator);
