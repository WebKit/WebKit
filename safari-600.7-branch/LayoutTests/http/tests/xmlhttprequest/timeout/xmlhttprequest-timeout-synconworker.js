if (this.document === undefined)
  importScripts("xmlhttprequest-timeout.js");

runTestRequests([ new RequestTracker(false, "no time out scheduled, load fires normally", 0),
		  new RequestTracker(false, "load fires normally", TIME_NORMAL_LOAD),
		  new RequestTracker(false, "timeout hit before load", TIME_REGULAR_TIMEOUT) ]);
