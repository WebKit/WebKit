description("Make sure WebSocket object acts as EventTarget.");

var ws = new WebSocket("ws://localhost:8000");
var open_event_handled = false;
var message_event_handled = false;
var close_event_handled = false;
function openListener() {
    open_event_handled = true;
};
function messageListener() {
    message_event_handled = true;
};
function closeListener() {
    close_event_handled = true;
}
ws.addEventListener('open', openListener, false);
ws.addEventListener('message', messageListener, false);
ws.addEventListener('close', closeListener, false);

shouldBe("open_event_handled", "false");
shouldBe("message_event_handled", "false");
shouldBe("close_event_handled", "false");

var evt = document.createEvent("Events");
evt.initEvent("open", true, false);
ws.dispatchEvent(evt);
shouldBe("open_event_handled", "true");

open_event_handled = false;
ws.removeEventListener('open', openListener);
ws.dispatchEvent(evt);
shouldBe("open_event_handled", "false");

evt = document.createEvent("MessageEvent");
evt.initEvent("message", true, false);
ws.dispatchEvent(evt);
shouldBe("message_event_handled", "true");

message_event_handled = false;
ws.removeEventListener('message', messageListener);
ws.dispatchEvent(evt);
shouldBe("message_event_handled", "false");

evt = document.createEvent("Events");
evt.initEvent("close", true, false);
ws.dispatchEvent(evt);
shouldBe("close_event_handled", "true");

close_event_handled = false;
ws.removeEventListener('close', closeListener);
ws.dispatchEvent(evt);
shouldBe("close_event_handled", "false");

var successfullyParsed = true;
