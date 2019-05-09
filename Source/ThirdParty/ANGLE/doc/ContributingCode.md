# Contributing Code

## Communicate

 * Whether you're writing a new feature or fixing an existing bug, it pays to get a second opinion before you get too far. If it's a new feature idea, post to the discussion group ([angleproject](https://groups.google.com/forum/?fromgroups#!forum/angleproject)) and propose it or talk with the ANGLE team on IRC in the #ANGLEproject channel on FreeNode.
 * Not all bugs in our [bug system](https://bugs.chromium.org/p/angleproject/issues/list) are assigned, but if the one you're interested in fixing is, send a note to the person it's assigned to and ask if they would like a patch.
 * Behavior changes and anything nontrivial (i.e. anything other than simple cleanups and style fixes) should generally be tracked in the bug system. Please [file a bug](http://anglebug.com/new) and describe what you're doing if there isn't one already.
 * If you would like bug-editing rights, simply ask a team member via email or the disussion group.

## Get your code ready
### Code
 1. Must conform to the [ANGLE style](CodingStandard.md) guidelines.
 2. Must be tested. (see the 'Testing' section below)
 3.  Should be a reasonable size to review.  Giant patches are unlikely to get reviewed quickly.

### Build maintenance
 1. If you added or removed source files:
    * You _must_ update the build files with your changes. See `src/libGLESv2.gni` and `src/compiler.gni`.
 2. ANGLE's BUILD.gn script is used by [Chromium's gn build](https://www.chromium.org/developers/gn-build-configuration). If you change build files other than to add or remove source files be aware you could break the Chromium build. ANGLE's commit queue (CQ) will detect such breakage. Ask a project member for help with Chromium issues if you don't have a Chromium checkout.
 3. If you modified `glslang.y` or `glslang.l`:
    * You _must_ update the bison-generated compiler sources. Download and install the latest 64-bit Bison and flex from official [Cygwin](https://cygwin.com/install.html) on _Windows_. From the Cygwin shell run `generate_parser.sh` in `src/compiler/translator` and update your CL. Do not edit the generated files by hand.
    * _NOTE:_ You can ignore failing chunk messages if there are no compile errors.
    * If you modified `ExpressionParser.y` or `Tokenizer.l`, follow the same process by running `src/compiler/preprocessor/generate_parser.sh`.

### Testing
 * ANGLE uses trybots to test on a variety of platforms. Please run your changes against our bots and check the results before landing changes or requesting reviews.
    * Upload your change (see [Making changes](#making-changes)).
    * To kick of a try job, use the 'CQ Dry Run' button, or set the Commit-Queue +1 label to trigger a dry run of the CQ (will not land the change).
    * If you are not part of the `angle-committers` group, you will need to either ask to be added or ask a member of the group to submit the tryjob for you. Add jmadill or geofflang as a reviewer for assistance.
    * Wait for the bots to report the result on the code review page. The bot results should be visible in Gerrit as yellow (in-progress), green (passed), or red (failed). This can take up to two hours for some of the debug bots. Click on the colored rectangle to open the bot log to triage failed tests.
    * If a failure is unexpected, or seems likely unrelated to your change, ask an ANGLE project member for advice.
    * We do not currently have the capability to run individual bots or tests in a run.
 * Tests can also be run locally, ANGLE's main testing methods are:
    * `angle_unittests`, `angle_end2end_tests` and `angle_white_box_tests` targets.
    * The [Top-of-Tree WebGL Conformance tests](https://www.khronos.org/registry/webgl/sdk/tests/webgl-conformance-tests.html).
      * If you are a Chromium developer, see [Building ANGLE for Chromium Development](BuildingAngleForChromiumDevelopment.md) for instructions on building ANGLE within Chromium.
      * If you aren't a browser developer, you should be able to drop your compiled DLLs into a Chrome installation, in place of those distributed with Chrome, to check WebGL conformance. [Chrome Canary](https://www.google.com/chrome/browser/canary.html) is well-suited for this.
    * If your code isn't covered by an existing test, you are *strongly encouraged* to add new test coverage. This both ensures that your code is correct and that new contributors won't break it in the future.
    * Add new tests to `angle_end2end_tests` for OpenGL-based API tests, `angle_unittests` for cross-platform internal tests, and `angle_white_box_tests` for rendering tests which also need visibility into internal ANGLE classes.
   * If you are submitting a performance fix, test your code with `angle_perftests` and add a new performance test if it is not covered by the existing benchmarks. For more documentation on `angle_perftests` see the [README](../src/tests/perf_tests/README.md).
   * The [Chromium GPU FYI bot waterfall](http://build.chromium.org/p/chromium.gpu.fyi/console) provides continuous integration for ANGLE patches that have been committed.  There may be hardware configurations that are not tested by the ANGLE trybots, if you notice breakage on this waterfall after landing a patch, please notify a project member.
   * ANGLE also includes the [drawElements Quality Program (dEQP)](dEQP.md) for additional testing. If you're working on a new feature, there may be some extensive tests for it already written.

### Legal
 1. You must complete the [Individual Contributor License Agreement](https://cla.developers.google.com/about/google-individual). You can do this online, and it only takes a minute. If you are contributing on behalf of a corporation, you must fill out the [Corporate Contributor License Agreement](https://cla.developers.google.com/about/google-corporate) and send it to Google as described on that page.
 2. Once you've submitted the CLA, please email the following information (as entered on the CLA) to `shannonwoods at chromium dot org` for record keeping purposes:
    * Full Name:
    * Email:
    * Company (If applicable):
 3. If you've never submitted code before, you must add your (or your organization's) name and contact info to the [AUTHORS](../AUTHORS) file.
 4. *NOTE TO REVIEWERS*: Follow the [External Contributor Checklist](http://www.chromium.org/developers/contributing-code/external-contributor-checklist).

## Life of a Change List

### Getting started with Gerrit for ANGLE
  1. Go to [https://chromium-review.googlesource.com/new-password](https://chromium-review.googlesource.com/new-password)
  2. Log in with the email you use for your git commits.
  3. Follow the directions on the new-password page to set up authentication with your Google account.
  4. Make sure to set your real name.
     * Visit [https://chromium-review.googlesource.com/#/settings](https://chromium-review.googlesource.com/#/settings) and check the "Full Name" field.
  5. Check out the repository (see [DevSetup](DevSetup.md)).
  6. Install the Gerrit `commit_msg` hook
     * Gerrit requires a hook to append a change ID tag to each commit, so that it can associate your CL with a particular review, and track dependencies between commits.
     * Download the hook from [https://chromium-review.googlesource.com/tools/hooks/commit-msg](https://chromium-review.googlesource.com/tools/hooks/commit-msg) and copy this file to `.git/hooks/commit-msg` within your local repository. On non-Windows, platforms, ensure that permissions are set to allow execution.
     * *BE AWARE:* Some patch management tools, such as StGit, currently bypass git hooks. They should not currently be used with changes intended for review.

### Making changes
 1. Commit your changes locally:
    * `git add src/../FileName.cpp`
    * `git commit`
    * A text editor will open. Add a description at the top of the file.
       * If your changes are associated with an issue in the issue tracker (e.g. a fix for a reported bug), please associate the CL with that issue by adding the following line to the commit message: `BUG=angleproject:<issue number>`.
    * Save.
    * Close the text editor.
    * Use `git commit --amend` to update your CL with new changes.
    * Use `git cl format` to amend the style of your CL. This saves both your time and the reviewers'!
 2. Ensure your code is landed on top of latest changes
    * `git pull --rebase`
    * Resolve conflicts if necessary
 3. Upload the change list
    * `git cl upload`
    * The change list and modified files will be uploaded to
      [ANGLE Gerrit](https://chromium-review.googlesource.com/q/project:angle/angle).
    * Follow the generated URL to the new issue.
    * Take a moment to perform a self-review of your code. Gerrit's viewer makes it easy to see whitespace errors, erroneous tabs, and other simple style problems.
    * [Select reviewers](#selecting-reviewers).  If you don't do this, reviewers may not realize you're requesting a review!
    * Make changes, upload and repeat as necessary.
    * Project members and others will review your code as described in the [CodeReviewProcess](CodeReviewProcess.md).
 5. If your change list needs revision:
    * If you have correctly installed the commit hook from the section above, Gerrit will be able to track your changes by Change-Id.
    * You should need only to update your commit with `git commit --amend` and re-upload with `git cl upload`.
 6. Landing change after it receives +2 Code Review:
    * If you are a committer, you may submit the change yourself via the Gerrit web interface.
    * If you are not a committer, ask your reviewer to submit the change list.
 7. Pull and integrate reviewed CL:
    * `git pull --rebase`

### Selecting reviewers
When your CL is ready to review, add any of the following reviewers. They will be able to route your CL to additional reviewers as neccssary and answer any questions you may have about the process.
 * `geofflang at chromium dot org`
 * `jmadill at chromium dot org`
 * `syoussefi at chromium dot org`
 * `ynovikov at chromium dot org`

### Committer status
Similar to [Chromium's committer status](https://dev.chromium.org/getting-involved/become-a-committer), long-term contributors to the ANGLE project may request to join the `angle-committers` group.  This allows you to give `+2` on code reviews and land patches without assistance.  After about 6 months of regular contributions, you may request committer status from a core ANGLE team member via email or code review.  Chromium committers may ask at any time.

See also:

* [ANGLE Gerrit](https://chromium-review.googlesource.com/q/project:angle/angle)
* [Chromium Projects: Contributing Code](http://www.chromium.org/developers/contributing-code/)
* [depot_tools tutorial](http://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html)
* [angle_perftests README](../src/tests/perf_tests/README.md)
