// For the current implementation of JavaScriptAudioNode, when it works with
// OfflineAudioContext (which runs much faster than real-time) the
// event.inputBuffer might be overwrite again before onaudioprocess ever get
// chance to be called. We carefully arrange the renderLengthInFrames and
// bufferSize to have exactly the same value to avoid this issue.
let renderLengthInFrames = 512;
let bufferSize = 512;

let context;

function createBuffer(context, numberOfChannels, length) {
  let audioBuffer = context.createBuffer(numberOfChannels, length, sampleRate);

  fillData(audioBuffer, numberOfChannels, audioBuffer.length);
  return audioBuffer;
}

function processAudioData(event, should) {
  buffer = event.outputBuffer;

  should(buffer.numberOfChannels, 'Number of channels in output buffer')
      .beEqualTo(outputChannels);
  should(buffer.length, 'Length of output buffer').beEqualTo(bufferSize);

  buffer = event.inputBuffer;

  let success = checkStereoOnlyData(buffer, inputChannels, buffer.length);

  should(success, 'onaudioprocess was called with the correct input data')
      .beTrue();
}

function fillData(buffer, numberOfChannels, length) {
  for (let i = 0; i < numberOfChannels; ++i) {
    let data = buffer.getChannelData(i);

    for (let j = 0; j < length; ++j)
      if (i < 2)
        data[j] = i * 2 - 1;
      else
        data[j] = 0;
  }
}

// Both 2 to 8 upmix and 8 to 2 downmix are just directly copy the first two
// channels and left channels are zeroed.
function checkStereoOnlyData(buffer, numberOfChannels, length) {
  for (let i = 0; i < numberOfChannels; ++i) {
    let data = buffer.getChannelData(i);

    for (let j = 0; j < length; ++j) {
      if (i < 2) {
        if (data[j] != i * 2 - 1)
          return false;
      } else {
        if (data[j] != 0)
          return false;
      }
    }
  }
  return true;
}

function runJSNodeTest(should) {
  // Create offline audio context.
  context = new OfflineAudioContext(2, renderLengthInFrames, sampleRate);

  let sourceBuffer =
      createBuffer(context, sourceChannels, renderLengthInFrames);

  let bufferSource = context.createBufferSource();
  bufferSource.buffer = sourceBuffer;

  let scriptNode =
      context.createScriptProcessor(bufferSize, inputChannels, outputChannels);

  bufferSource.connect(scriptNode);
  scriptNode.connect(context.destination);
  scriptNode.onaudioprocess = event => {
    processAudioData(event, should);
  };

  bufferSource.start(0);
  return context.startRendering();
}
