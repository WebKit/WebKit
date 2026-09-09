// report() is provided by whichever page loads this script. The frames that use it lose their
// inspector connection partway through breakpoint evaluation, so they report by postMessage to
// their parent rather than over the protocol.
//
// setBreakpointInFrameScript() in breakpoint-detach-test-utilities.js breaks on the report() line
// below and addresses it by line number, so keep the two in sync when editing this file.
function functionWithBreakpoint()
{
    report("iframe: breakpoint line ran");
}
