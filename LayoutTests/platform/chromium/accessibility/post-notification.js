function enumAccessibilityObjects(accessibilityObject) {
  var count = accessibilityObject.childrenCount;
  for (var i = 0; i < count; ++i)
    enumAccessibilityObjects(accessibilityObject.childAtIndex(i));
}

function testDone() {
  layoutTestController.notifyDone();
}

function test() {
  layoutTestController.dumpAsText();
  layoutTestController.waitUntilDone();
  
  // Build accessibility tree.
  document.body.focus();
  enumAccessibilityObjects(accessibilityController.focusedElement);

  // Test the accessibility notification.
  accessibilityController.dumpAccessibilityNotifications();
  testNotification();

  // Use setTimeout so that asynchronous accessibility notifications can be processed.
  setTimeout("setTimeout(testDone, 0);", 0);
}
