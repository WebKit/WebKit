object = null;
count = 0
onmessage = function(message) {
    try {
        if (!object)
            object = {[message.data.filename]: message.data.filename};
        if (count++ == 10) {
            object = null;
            for (var i = 0; i < 1000; i++)
                object = {} + "" + i
            object = null;
            count = 0;
        }
        postMessage(message.data);
    } catch(e) {
        postMessage(message.data);
    }
}

