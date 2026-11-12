// META: title=Transfer port during message dispatch preserves remaining messages
// META: timeout=long

async_test(function(t) {
  var channel1 = new MessageChannel();
  var channel2 = new MessageChannel();

  // Post multiple messages to port2 via port1.
  channel1.port1.postMessage("first");
  channel1.port1.postMessage("second");
  channel1.port1.postMessage("third");

  var receivedAtOld = [];
  var receivedAtNew = [];

  // Start port2. In the handler for the first message, transfer port2.
  channel1.port2.onmessage = t.step_func(function(evt) {
    receivedAtOld.push(evt.data);
    // Transfer port2 upon receiving the first message.
    // Per spec, remaining messages in the port message queue should
    // move to the new location.
    channel2.port1.postMessage("transfer", [channel1.port2]);
  });

  // At the receiving end, set up a handler on the transferred port.
  channel2.port2.onmessage = t.step_func(function(evt) {
    var transferred = evt.ports[0];
    transferred.onmessage = t.step_func(function(evt) {
      receivedAtNew.push(evt.data);
      if (receivedAtNew.length + receivedAtOld.length === 3) {
        // All three messages accounted for.
        assert_equals(receivedAtOld.length, 1,
          "Only the first message should be received at the old location");
        assert_equals(receivedAtOld[0], "first");
        assert_equals(receivedAtNew.length, 2,
          "Remaining messages should be received at the new location");
        assert_equals(receivedAtNew[0], "second");
        assert_equals(receivedAtNew[1], "third");
        t.done();
      }
    });
  });

  // Timeout if messages are lost.
  t.step_timeout(function() {
    assert_unreached(
      "Timed out. Received at old: [" + receivedAtOld + "], " +
      "received at new: [" + receivedAtNew + "]. " +
      "Total: " + (receivedAtOld.length + receivedAtNew.length) + "/3. " +
      "Messages were likely lost during transfer.");
  }, 2000);
}, "Transferring a port during message dispatch preserves remaining queued messages");
