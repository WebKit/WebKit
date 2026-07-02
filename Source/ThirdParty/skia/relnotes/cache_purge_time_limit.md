* The following new feature is only supported by the Graphite backend.

* Allow clients to pass in a maximum time duration Skia should spend when purging purgeable
  resources from caches. Skia will purge available purgeable resources until all purgeable resources
  are removed *or* until the purge time has been surpassed, at which point we exit early.

* Clients can either call the Context or the Recorder's performDeferredCleanup(...) method with a
  max duration in microseconds. Following the current implementation pattern, durations passed in
  get converted to time points when calling in to the ResourceCache itself.

* Public API calls into performDeferredCleanup accept an optional stop time duration which is set to
  std::nullopt by default.  This signifies no time limit for purging and means this change should
  not disrupt current client API calls nor impact existing functionality.
