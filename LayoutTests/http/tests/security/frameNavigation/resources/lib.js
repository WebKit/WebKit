// Create an object to hold the navUtils API.
var navUtils = {};

// A pointer to a DOM node at which we direct log messages.
navUtils.console_ = null;

// The number of frames used in this test.
navUtils.frame_count_ = 0;
navUtils.set_frame_count = function(n) { navUtils.frame_count_ = n; };

// The number of ready messages we've seen.
navUtils.ready_count_ = 0;

// The number of done messages we've seen.
navUtils.done_count_ = 0;

// The URL to which we navigate frames.
navUtils.target_url_ =
    'http://127.0.0.1:8000/security/frameNavigation/resources/target.html';
navUtils.set_target_url = function(url) { navUtils.target_url_ = url; };

// Log a message to the console.
navUtils.log = function(msg) {
  if (!navUtils.console_)
    navUtils.console_ = document.getElementById('console');

  var div = document.createElement('div');
  div.appendChild(document.createTextNode(msg));
  navUtils.console_.appendChild(div);
};

navUtils.logWindowCount = function() {
  if (!window.layoutTestController) {
    navUtils.log('layoutTestController unavailable.');
    return;
  }

  navUtils.log('Window count: ' + layoutTestController.windowCount());
};

navUtils.getController = function() {
  var controller = window.top;
  while (controller.opener)
    controller = controller.opener.top;

  return controller;
};

navUtils.send = function(msg) {
  navUtils.getController().postMessage(msg, '*');
};

navUtils.doNavigate = function(wnd, mechanism, target) {
  wnd.postMessage('{"status": "navigate", "mechanism": "' + mechanism + '", "target": "' + target + '"}', '*');
}

navUtils.canAccess = function(wnd) {
  try {
    var html = wnd.document.documentElement.innerHTML;
    return true;
  } catch(ex) {
    return false;
  }
};

//////////////////////////////////////////////////////////
// Navigation commands.

navUtils.navigateByLocation = function(wnd) {
  window.setTimeout(function() {
    wnd.location = navUtils.target_url_;
  }, 0);
};

navUtils.navigateByOpen = function(name) {
  window.setTimeout(function() {
    window.open(navUtils.target_url_, name);
  }, 0);
};

navUtils.navigateByForm = function(name) {
  window.setTimeout(function() {
    // Create the form.
    var form = document.createElement('form');
    form.action = navUtils.target_url_;
    form.method = 'POST';
    form.target = name;
    document.body.appendChild(form);

    // Submit the form.
    form.submit();
  }, 0);
};

navUtils.navigateByHyperlink = function(name) {
  window.setTimeout(function() {
    // Create the link.
    var link = document.createElement('a');
    link.href = navUtils.target_url_;
    link.target = name;
    document.body.appendChild(link);

    // Submit the link.
    var evt = document.createEvent('MouseEvent');
    evt.initEvent('click', true, true);
    link.dispatchEvent(evt);
  }, 0);
};

navUtils.navigateByPlugin = function(name) {
  window.setTimeout(function() {
    if (!window.layoutTestController) {
      navUtils.send('{"status": "log", "message": "Warning: Test plugin not available!"}');
      navUtils.navigateByOpen(name);
      return;
    }
    // Create the plugin.
    var plg = document.createElement('embed');
    plg.type = 'application/x-webkit-test-netscape'
    document.body.appendChild(plg);

    // Use the plugin.
    plg.getURL(navUtils.target_url_, name);
  }, 0);
};

//////////////////////////////////////////////////////////
// Callbacks

// Called when all the frames have reported that they are ready.
navUtils.onready = function() { };

// Called when all the frames have reported that they are done.
navUtils.ondone = function() { };

window.addEventListener('message', function(ev) {
  var msg = eval('(' + ev.data + ')');

  switch(msg.status) {
  case 'ready':
    ++navUtils.ready_count_;
    if (navUtils.ready_count_ >= navUtils.frame_count_)
      window.setTimeout(navUtils.onready, 0);
    break;

  case 'done':
    ++navUtils.done_count_;
    if (navUtils.done_count_ >= navUtils.frame_count_)
      window.setTimeout(navUtils.ondone, 0);
    break;

  case 'log':
    navUtils.log(msg.message);
    break;

  case 'navigate':
    switch(msg.mechanism) {
    case 'location':
      navUtils.navigateByLocation(eval(msg.target));
      break;
    case 'open':
      navUtils.navigateByOpen(msg.target);
      break;
    case 'form':
      navUtils.navigateByForm(msg.target);
      break;
    case 'plugin':
      navUtils.navigateByPlugin(msg.target);
      break;
    case 'hyperlink':
      navUtils.navigateByHyperlink(msg.target);
      break;
    }
    break;
  }
}, false);

// Initialize the layoutTestController
if (window.self === window.top) {
  if (window.layoutTestController) {
    layoutTestController.waitUntilDone();
    layoutTestController.setCanOpenWindows();
    layoutTestController.dumpAsText();
    layoutTestController.dumpChildFramesAsText();
    layoutTestController.setCloseRemainingWindowsWhenComplete(true);
  }
}

