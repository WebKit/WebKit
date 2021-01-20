var status = "no status";

function stringToBuffer(value) {
    var result = new ArrayBuffer(value.length*2);
    var view = new Uint16Array(result);
    for (var i = 0; i < value.length; ++i)
        view[i] = value.charCodeAt(i);
    return result;
}

self.addEventListener("fetch", (event) => {
    if (event.request.url.indexOf("status") !== -1) {
        event.respondWith(new Response(null, {status: 200, statusText: status}));
        return;
    }
    if (event.request.url.endsWith(".fromserviceworker")) {
        status = event.request.url.substring(0, event.request.url.length - 18) + " through " + "fetch";
        event.respondWith(fetch(event.request.url.substring(0, event.request.url.length - 18)));
    }
    if (event.request.url.endsWith(".bodyasanemptystream")) {
        var stream = new ReadableStream({ start : controller => {
            controller.close();
        }});
        event.respondWith(new Response(stream, {status : 200, statusText : "Empty stream"}));
        return;
    }
    if (event.request.url.endsWith(".bodyasstream")) {
        var stream = new ReadableStream({ start : async controller => {
            await controller.enqueue(stringToBuffer("This "));
            await controller.enqueue(stringToBuffer("test "));
            await controller.enqueue(stringToBuffer("passes "));
            await controller.enqueue(stringToBuffer("if "));
            await controller.enqueue(stringToBuffer("the sentence "));
            await controller.enqueue(stringToBuffer("is "));
            await controller.enqueue(stringToBuffer("complete "));
            await controller.enqueue(stringToBuffer("with "));
            await controller.enqueue(stringToBuffer("PASS."));
            controller.close();
        }});
        event.respondWith(new Response(stream, {status : 200, statusText : "Empty stream"}));
        return;
    }
    state = "unknown url";
    event.respondWith(new Response(null, {status: 404, statusText: "Not Found"}));
    return;
});
