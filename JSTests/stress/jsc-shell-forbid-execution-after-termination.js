//@ runDefault("--watchdog=50", "--watchdog-exception-ok")
Promise.resolve().then(()=>''.localeCompare());
''.localeCompare();
