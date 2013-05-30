if (this.document === undefined)
  importScripts("xmlhttprequest-timeout.js");

runTestRequests([ new AbortedRequest(false),
		  new AbortedRequest(true, -1),
		  new AbortedRequest(true, TIME_NORMAL_LOAD) ]);
