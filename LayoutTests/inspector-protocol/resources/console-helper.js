window.ConsoleHelper = {};

ConsoleHelper.simplifiedConsoleMessage = function(messageObject)
{
    function basename(url)
    {
        return url.substring(url.lastIndexOf("/") + 1) || "???";
    }

    var message = messageObject.params.message;
    var obj = {
        source: message.source,
        level: message.level,
        text: message.text,
        location: basename(message.url) + ":" + message.line + ":" + message.column
    };

    if (message.parameters) {
        var params = [];
        for (var i = 0; i < message.parameters.length; ++i) {
            var param = message.parameters[i];
            var o = {type: param.type};
            if (param.subtype)
                o.subtype = param.subtype;
            params.push(o);
        }
        obj.parameters = params;
    }

    return obj;
}
