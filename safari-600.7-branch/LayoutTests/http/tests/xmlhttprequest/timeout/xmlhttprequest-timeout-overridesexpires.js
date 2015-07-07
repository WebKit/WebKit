if (this.document === undefined)
  importScripts("xmlhttprequest-timeout.js");

runTestRequests([ new RequestTracker(true, "timeout set to expiring value after load fires", TIME_NORMAL_LOAD, TIME_LATE_TIMEOUT, TIME_DELAY),
          // FIXME: http://webkit.org/b/98156 - Late updates are not supported yet, this test is not run.
		  // new RequestTracker(true, "timeout set to expired value before load fires", TIME_NORMAL_LOAD, TIME_REGULAR_TIMEOUT, TIME_DELAY),
		  new RequestTracker(true, "timeout set to non-expiring value after timeout fires", TIME_DELAY, TIME_REGULAR_TIMEOUT, TIME_NORMAL_LOAD) ]);
