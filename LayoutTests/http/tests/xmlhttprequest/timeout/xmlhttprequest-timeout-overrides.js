if (this.document === undefined)
  importScripts("xmlhttprequest-timeout.js");

runTestRequests([ new RequestTracker(true, "timeout disabled after initially set", TIME_NORMAL_LOAD, TIME_REGULAR_TIMEOUT, 0),
		  // new RequestTracker(true, "timeout overrides load after a delay", TIME_NORMAL_LOAD, TIME_DELAY, TIME_REGULAR_TIMEOUT),
		  new RequestTracker(true, "timeout enabled after initially disabled", 0, TIME_REGULAR_TIMEOUT, TIME_NORMAL_LOAD) ]);
