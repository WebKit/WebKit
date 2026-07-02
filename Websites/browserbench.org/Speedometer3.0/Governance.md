# Speedometer Governance Policy

Speedometer uses multistakeholder governance.
This will allow us to share work, and build a collaborative understanding of performance on the web
in order to drive resourcing towards appropriate areas.
This also provides a structure that can endure to provide maintenance and adapt to the future web.

An eligible “browser project” is a core end-to-end web browser engine with an integrated JavaScript engine
which distributes implementations widely. The project may delegate decision making within Speedometer
to multiple representatives (for example, to review code commits or to provide consensus for major changes).
The participating browser projects at this time are Blink/V8, Gecko/SpiderMonkey, and WebKit/JavaScriptCore.

The intent is that the working team should be able to move quickly for most changes,
with a higher level of process and consensus expected based on the impact of the change.

-   **Trivial change** - This is a change that has no effect on the official benchmark and includes changes
    to whitespaces, comments, documentation outside policies and governance model, and unofficial test cases.
    A trivial change requires approval by a reviewer, who is not the author of the change,
    from one of the participating browser projects.
    The intent is to ensure basic code quality & license compatibility, not to reach agreement.
    For example, one participating browser project might be both writing and reviewing a new benchmark in
    a subfolder to test in their own CI, or reviewing code written by an external contributor.
-   **Non-trivial change** - This is a change that has small impact on the official benchmark and includes
    changes to official test cases, test runners, bug fixes, and the appearance of the benchmark.
    A non-trivial change requires approval by at least two of the participating browser projects
    (including either authoring or reviewing the change) and none other strongly opposed to the change
    within 10 business days.
-   **Major change** - This is a change that has major implications on the official benchmark such as
    releasing of a new version of the benchmark or any revisions to governance policies and processes,
    including changes to the participating browser projects.
    A major change requires a consensus, meaning approvals by each of the participating browser projects.

This governance policy and associated code will be hosted inside the Speedometer repository within
the WebKit GitHub organization under the 2-clause BSD license.
