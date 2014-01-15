window.ProbeHelper = {};

ProbeHelper.simplifiedProbeSample = function(messageObject)
{
    var data = messageObject.params.sample;
    return {
        probeId: data.probeId,
        batchId: data.batchId,
        sampleId: data.sampleId,
        payload: data.payload
    };
}
