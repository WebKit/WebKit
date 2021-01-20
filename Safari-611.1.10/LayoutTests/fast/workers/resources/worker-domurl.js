function log(message)
{
    postMessage(message);
}

onmessage = function(event)
{
    if (URL.createObjectURL == undefined)
        log('FAIL: URL.createObjectURL undefined');
    else
        log('PASS: URL.createObjectURL defined');

    if (URL.revokeObjectURL == undefined)
        log('FAIL: URL.revokeObjectURL undefined');
    else
        log('PASS: URL.revokeObjectURL defined');

    log('DONE');
}
