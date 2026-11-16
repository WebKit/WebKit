//@ memoryHog!
//@requireOptions("--watchdog=1000", "--watchdog-exception-ok")

new Uint8Array({length: 2**32});
