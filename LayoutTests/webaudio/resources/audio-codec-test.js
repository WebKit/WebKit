var defaultSampleRate = 44100.0;
var lengthInSeconds = 1;

var context = 0;
var bufferLoader = 0;

// Run test by loading the file specified by |url|.  An optional sample rate can be given to
// select a context with a different sample rate.  The default value is |defaultSampleRate|.
function runDecodingTest(url, optionalSampleRate) 
{
    if (!window.layoutTestController)
        return;

    var sampleRate = (typeof optionalSampleRate === "undefined") ? defaultSampleRate : optionalSampleRate;

    // Create offline audio context.
    context = new webkitAudioContext(1, sampleRate * lengthInSeconds, sampleRate);
    
    bufferLoader = new BufferLoader(
        context,
        [ url ],
        finishedLoading
    );
    
    bufferLoader.load();
    layoutTestController.waitUntilDone();
}

function finishedLoading(bufferList)
{
    layoutTestController.setAudioData(createAudioData(bufferList[0]));
    layoutTestController.notifyDone();
}

