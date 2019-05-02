//@ runWithoutBaseOption("default", "--validateExceptionChecks=1")

// This test that we have appropriate exception check processing Proxy.getOwnPropertySlots

function foo()
{
}

let a = {...new Proxy(foo, {})}
