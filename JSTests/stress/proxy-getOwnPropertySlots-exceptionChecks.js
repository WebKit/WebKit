//@ skip
// Skipping this test while https://bugs.webkit.org/show_bug.cgi?id=197485 is being fixed
//   @ runWithoutBaseOption("default", "--validateExceptionChecks=1")

// This test that we have appropriate exception check processing Proxy.getOwnPropertySlots

function foo()
{
}

let a = {...new Proxy(foo, {})}
