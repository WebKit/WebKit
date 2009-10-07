description("This tests the init functions for all the event DOM classes that have them.");

function testInitEvent(prefix, argumentString)
{
    var event = document.createEvent(prefix + "Event");
    var initExpression = "event.init" + prefix + "Event(" + argumentString + ")";
    eval(initExpression);
    return event;
}

var event = document.createEvent("Event");

event.initEvent("type", false, false);

shouldBe("testInitEvent('', '\"a\", false, false').type", "'a'");
shouldBe("testInitEvent('', 'null, false, false').type", "'null'");
shouldBe("testInitEvent('', '\"a\", false, false').bubbles", "false");
shouldBe("testInitEvent('', '\"a\", true, false').bubbles", "true");
shouldBe("testInitEvent('', '\"a\", false, false').cancelable", "false");
shouldBe("testInitEvent('', '\"a\", false, true').cancelable", "true");

shouldBe("testInitEvent('Keyboard', '\"a\", false, false, window, \"b\", 1001, false, false, false, false, false').type", "'a'");
shouldBe("testInitEvent('Keyboard', 'null, false, false, window, \"b\", 1001, false, false, false, false, false').type", "'null'");
shouldBe("testInitEvent('Keyboard', '\"a\", false, false, window, \"b\", 1001, false, false, false, false, false').bubbles", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", true, false, window, \"b\", 1001, false, false, false, false, false').bubbles", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, false, window, \"b\", 1001, false, false, false, false, false').cancelable", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').cancelable", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').view", "window");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, null, \"b\", 1001, false, false, false, false, false').view", "null");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').keyIdentifier", "'b'");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, null, 1001, false, false, false, false, false').keyIdentifier", "'null'");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').keyLocation", "1001");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').ctrlKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, true, false, false, false, false').ctrlKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').altKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, true, false, false, false').altKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').shiftKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, true, false, false').shiftKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').metaKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, true, false').metaKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').altGraphKey", "false");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, true').altGraphKey", "true");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, true').detail", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').keyCode", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').charCode", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').layerX", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').layerY", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').pageX", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').pageY", "0");
shouldBe("testInitEvent('Keyboard', '\"a\", false, true, window, \"b\", 1001, false, false, false, false, false').which", "0");

shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, null').type", "'a'");
shouldBe("testInitEvent('Message', 'null, false, false, \"b\", \"c\", \"d\", window, null').type", "'null'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, null').bubbles", "false");
shouldBe("testInitEvent('Message', '\"a\", true, false, \"b\", \"c\", \"d\", window, null').bubbles", "true");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, null').cancelable", "false");
shouldBe("testInitEvent('Message', '\"a\", false, true, \"b\", \"c\", \"d\", window, null').cancelable", "true");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, null').data", "'b'");
shouldBe("testInitEvent('Message', '\"a\", false, false, null, \"c\", \"d\", window, null').data", "null");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, null').origin", "'c'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", null, \"d\", window, null').origin", "'null'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, null').lastEventId", "'d'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", null, window, null').lastEventId", "'null'");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", window, null').source", "window");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", null, null').source", "null");
var channel = new MessageChannel;
var channelArray = [channel.port1, channel.port2];
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", null, channelArray').ports[0]", "channel.port1");
shouldBe("testInitEvent('Message', '\"a\", false, false, \"b\", \"c\", \"d\", null, channelArray').ports[1]", "channel.port2");

shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').type", "'a'");
shouldBe("testInitEvent('Mouse', 'null, false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').type", "'null'");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').bubbles", "false");
shouldBe("testInitEvent('Mouse', '\"a\", true, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').bubbles", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').cancelable", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, true, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').cancelable", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').view", "window");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, null, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').view", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').detail", "1001");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').screenX", "1002");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').screenY", "1003");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').clientX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').clientY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').ctrlKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, true, false, false, false, 1006, null').ctrlKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').altKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, true, false, false, 1006, null').altKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').shiftKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, true, false, 1006, null').shiftKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').metaKey", "false");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, true, 1006, null').metaKey", "true");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').button", "1006");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').relatedTarget", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, document').relatedTarget", "document");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').offsetX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').offsetY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').x", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').y", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').fromElement", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, document').fromElement", "document");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').toElement", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').dataTransfer", "null");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').keyCode", "0");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').charCode", "0");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').layerX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').layerY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').pageX", "1004");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').pageY", "1005");
shouldBe("testInitEvent('Mouse', '\"a\", false, false, window, 1001, 1002, 1003, 1004, 1005, false, false, false, false, 1006, null').which", "1007");

shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').type", "'a'");
shouldBe("testInitEvent('Mutation', 'null, false, false, null, \"b\", \"c\", \"d\", 1001').type", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').bubbles", "false");
shouldBe("testInitEvent('Mutation', '\"a\", true, false, null, \"b\", \"c\", \"d\", 1001').bubbles", "true");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').cancelable", "false");
shouldBe("testInitEvent('Mutation', '\"a\", false, true, null, \"b\", \"c\", \"d\", 1001').cancelable", "true");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').relatedNode", "null");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, document, \"b\", \"c\", \"d\", 1001').relatedNode", "document");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').prevValue", "'b'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, null, \"c\", \"d\", 1001').prevValue", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').newValue", "'c'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", null, \"d\", 1001').newValue", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').attrName", "'d'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", null, 1001').attrName", "'null'");
shouldBe("testInitEvent('Mutation', '\"a\", false, false, null, \"b\", \"c\", \"d\", 1001').attrChange", "1001");

// initOverflowEvent has an interface that has an design that's inconsistent with the init functions from other events.
shouldBe("testInitEvent('Overflow', '1001, false, false').type", "'overflowchanged'");
shouldBe("testInitEvent('Overflow', '1001, false, false').bubbles", "false");
shouldBe("testInitEvent('Overflow', '1001, false, false').cancelable", "false");
shouldBe("testInitEvent('Overflow', '1001, false, false').orient", "1001");
shouldBe("testInitEvent('Overflow', '1001, false, false').horizontalOverflow", "false");
shouldBe("testInitEvent('Overflow', '1001, true, false').horizontalOverflow", "true");
shouldBe("testInitEvent('Overflow', '1001, false, false').verticalOverflow", "false");
shouldBe("testInitEvent('Overflow', '1001, false, true').verticalOverflow", "true");

shouldBe("testInitEvent('Progress', '\"a\", false, false, false, 1001, 1002').type", "'a'");
shouldBe("testInitEvent('Progress', 'null, false, false, false, 1001, 1002').type", "'null'");
shouldBe("testInitEvent('Progress', '\"a\", false, false, false, 1001, 1002').bubbles", "false");
shouldBe("testInitEvent('Progress', '\"a\", true, false, false, 1001, 1002').bubbles", "true");
shouldBe("testInitEvent('Progress', '\"a\", false, false, false, 1001, 1002').cancelable", "false");
shouldBe("testInitEvent('Progress', '\"a\", false, true, false, 1001, 1002').cancelable", "true");
shouldBe("testInitEvent('Progress', '\"a\", false, false, false, 1001, 1002').lengthComputable", "false");
shouldBe("testInitEvent('Progress', '\"a\", false, false, true, 1001, 1002').lengthComputable", "true");
shouldBe("testInitEvent('Progress', '\"a\", false, false, false, 1001, 1002').loaded", "1001");
shouldBe("testInitEvent('Progress', '\"a\", false, false, false, 1001, 1002').total", "1002");

shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').type", "'a'");
shouldBe("testInitEvent('Storage', 'null, false, false, \"b\", \"c\", \"d\", \"e\", window').type", "'null'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').bubbles", "false");
shouldBe("testInitEvent('Storage', '\"a\", true, false, \"b\", \"c\", \"d\", \"e\", window').bubbles", "true");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').cancelable", "false");
shouldBe("testInitEvent('Storage', '\"a\", false, true, \"b\", \"c\", \"d\", \"e\", window').cancelable", "true");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').key", "'b'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, null, \"c\", \"d\", \"e\", window').key", "'null'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').oldValue", "'c'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", null, \"d\", \"e\", window').oldValue", "null");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').newValue", "'d'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", null, \"e\", window').newValue", "null");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').uri", "'e'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", null, window').uri", "'null'");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", window').source", "window");
shouldBe("testInitEvent('Storage', '\"a\", false, false, \"b\", \"c\", \"d\", \"e\", null').source", "null");

shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').type", "'a'");
shouldBe("testInitEvent('Text', 'null, false, false, window, \"b\"').type", "'null'");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').bubbles", "false");
shouldBe("testInitEvent('Text', '\"a\", true, false, window, \"b\"').bubbles", "true");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').cancelable", "false");
shouldBe("testInitEvent('Text', '\"a\", false, true, window, \"b\"').cancelable", "true");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').view", "window");
shouldBe("testInitEvent('Text', '\"a\", false, false, null, \"b\"').view", "null");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').data", "'b'");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, null').data", "'null'");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').detail", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').keyCode", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').charCode", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').layerX", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').layerY", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').pageX", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').pageY", "0");
shouldBe("testInitEvent('Text', '\"a\", false, false, window, \"b\"').which", "0");

shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').type", "'a'");
shouldBe("testInitEvent('UI', 'null, false, false, window, 1001').type", "'null'");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').bubbles", "false");
shouldBe("testInitEvent('UI', '\"a\", true, false, window, 1001').bubbles", "true");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').cancelable", "false");
shouldBe("testInitEvent('UI', '\"a\", false, true, window, 1001').cancelable", "true");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').view", "window");
shouldBe("testInitEvent('UI', '\"a\", false, false, null, 1001').view", "null");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').detail", "1001");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').keyCode", "0");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').charCode", "0");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').layerX", "0");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').layerY", "0");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').pageX", "0");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').pageY", "0");
shouldBe("testInitEvent('UI', '\"a\", false, false, window, 1001').which", "0");

shouldBe("testInitEvent('WebKitAnimation', '\"a\", false, false, \"b\", 1001').type", "'a'");
shouldBe("testInitEvent('WebKitAnimation', 'null, false, false, \"b\", 1001').type", "'null'");
shouldBe("testInitEvent('WebKitAnimation', '\"a\", false, false, \"b\", 1001').bubbles", "false");
shouldBe("testInitEvent('WebKitAnimation', '\"a\", true, false, \"b\", 1001').bubbles", "true");
shouldBe("testInitEvent('WebKitAnimation', '\"a\", false, false, \"b\", 1001').cancelable", "false");
shouldBe("testInitEvent('WebKitAnimation', '\"a\", false, true, \"b\", 1001').cancelable", "true");
shouldBe("testInitEvent('WebKitAnimation', '\"a\", false, false, \"b\", 1001').animationName", "'b'");
shouldBe("testInitEvent('WebKitAnimation', '\"a\", false, false, null, 1001').animationName", "'null'");
shouldBe("testInitEvent('WebKitAnimation', '\"a\", false, false, \"b\", 1001').elapsedTime", "1001");

shouldBe("testInitEvent('WebKitTransition', '\"a\", false, false, \"b\", 1001').type", "'a'");
shouldBe("testInitEvent('WebKitTransition', 'null, false, false, \"b\", 1001').type", "'null'");
shouldBe("testInitEvent('WebKitTransition', '\"a\", false, false, \"b\", 1001').bubbles", "false");
shouldBe("testInitEvent('WebKitTransition', '\"a\", true, false, \"b\", 1001').bubbles", "true");
shouldBe("testInitEvent('WebKitTransition', '\"a\", false, false, \"b\", 1001').cancelable", "false");
shouldBe("testInitEvent('WebKitTransition', '\"a\", false, true, \"b\", 1001').cancelable", "true");
shouldBe("testInitEvent('WebKitTransition', '\"a\", false, false, \"b\", 1001').propertyName", "'b'");
shouldBe("testInitEvent('WebKitTransition', '\"a\", false, false, null, 1001').propertyName", "'null'");
shouldBe("testInitEvent('WebKitTransition', '\"a\", false, false, \"b\", 1001').elapsedTime", "1001");

// WheelEvent has no init function yet; roughly speaking, we are waiting for that part of DOM 3 to stabilize.

var successfullyParsed = true;
