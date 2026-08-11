Want to contribute? Great! First, read this page (including the small print at the end).

# Before you contribute
Before we can use your code, you must sign the
[Google Individual Contributor License Agreement](https://cla.developers.google.com/about/google-individual)
(CLA), which you can do online. The CLA is necessary mainly because you own the
copyright to your changes, even after your contribution becomes part of our
codebase, so we need your permission to use and distribute your code. We also
need to be sure of various other things—for instance that you'll tell us if you
know that your code infringes on other people's patents. You don't have to sign
the CLA until after you've submitted your code for review and a member has
approved it, but you must do it before we can put your code into our codebase.
Before you start working on a larger contribution, you should get in touch with
us first via email with your idea so that we can help out and possibly guide
you. Coordinating up front makes it much easier to avoid frustration later on.

# Code reviews
All submissions, including submissions by project members, require review. We
use [Gerrit](https://boringssl-review.googlesource.com) for this purpose.

## Setup
If you have not done so on this machine, you will need to set up a password for
Gerrit. Sign in with a Google account, visit
[this link](https://boringssl.googlesource.com/), and click the "Generate
Password" link in the top right.

You must also have a Gerrit account associated with
your Google account.  To do this visit the [Gerrit review server](https://boringssl-review.googlesource.com)
and click "Sign in" (top right).

## Uploading changes

There are a few different workflows for uploading to Gerrit, depending on what
tools you have available and whether you are more familiar with Gerrit or
Chromium's `depot_tools`.

**WARNING**: The two workflows, by default, convert branches and commits into
code review changes very differently.

### Uploading directly to Gerrit

To use the Gerrit workflow, you will need to prepare your checkout to
[add Change-Ids](https://gerrit-review.googlesource.com/Documentation/cmd-hook-commit-msg.html)
on commit. Run:

    curl -Lo .git/hooks/commit-msg https://boringssl-review.googlesource.com/tools/hooks/commit-msg
    chmod u+x .git/hooks/commit-msg

To upload a change, push it to the special `refs/for/main` target:

    git push origin HEAD:refs/for/main

The output will then give you a link to the change. Add `agl@google.com`,
`davidben@google.com` as reviewers.

Pushing a commit with the same `Change-Id` as an existing change will upload a new
version of it. (Gerrit refers to versions of a change as "patchsets".) The
`git rebase` or `git commit --amend` commands may be helpful to modify an
existing commit. Making changes as separate commits will result in multiple
Gerrit change, as described below.

Pushing a series of commits will create a series of dependent changes. To upload
new versions of commits in the series, `git rebase -i` may be helpful.

For more detailed instructions, see the
[Gerrit User Guide](https://gerrit-review.googlesource.com/Documentation/intro-user.html).
Google employers may also find [go/gerrit-dev-workflows](https://goto.corp.google.com/gerrit-dev-workflows)
helpful.

### Uploading using depot_tools

If you have Chromium's `depot_tools` installed, you can use `git cl upload` to
upload a change. This also has the advantage of automatically running any
relevant `PRESUBMIT.py` checks, which can catch common/trivial errors locally at
upload time instead of waiting for a slower CQ run.

By default, your entire branch will be squashed into a single Gerrit change.
This avoids the need for tools like `git commit --amend` to upload new versions
of the change. However, to upload a series of changes, you must create a series
of branches in your repository. Setting `gerrit.squash-uploads` to `false` in
`git config` disables this behavior and causes `git cl upload` to behave like
the Gerrit workflow above.

See [depot_tools
documentation](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools.html)
for more info.

# Copyright headers
New files contributed directly to BoringSSL should use the following copyright
header, where `YEAR` is the year the file was added:

    Copyright YEAR The BoringSSL Authors

To list individual contributors, we maintain an [AUTHORS](./AUTHORS) file at the
top level of the project. See [this documentation](https://opensource.google/documentation/reference/releasing/authors)
for more details. If you wish to be added, you are welcome to add yourself as
part of your contribution, or request that we add you.

We started the AUTHORS file after the project began, and after receiving many
valuable contributions. To avoid being presumptuous, we did not proactively list
all past contributors. If you previously made a contribution, you are likewise
welcome to send us a patch to be added, or request that we add you.

After the copyright lines, files should include the license notice described in
the Apache 2.0 appendix. Thus new files should begin with the following header:

    // Copyright YEAR The BoringSSL Authors
    //
    // Licensed under the Apache License, Version 2.0 (the "License");
    // you may not use this file except in compliance with the License.
    // You may obtain a copy of the License at
    //
    //     https://www.apache.org/licenses/LICENSE-2.0
    //
    // Unless required by applicable law or agreed to in writing, software
    // distributed under the License is distributed on an "AS IS" BASIS,
    // WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    // See the License for the specific language governing permissions and
    // limitations under the License.

# Testing
See the [build instructions](./BUILDING.md) for instructions on how to run
tests.

Additionally, our Gerrit instance is configured to run our tests on a range of
platforms. This is called the "commit queue" or CQ. Project members can set the
`Commit-Queue` label to +1 for a dry run, which runs the tests without
submitting the CL.

# Pre-generated files
There are a number of files in BoringSSL which are checked into the source tree,
to reduce dependencies for consumers of the library. When modifying their
inputs, the generated files must be updated. The CQ and `depot_tools` presubmit
scripts will check that they are correct.

See [pre-generated file documentation](./gen/README.md) for how to update these
files.

# The small print
Contributions made by corporations are covered by a different agreement than
the one above, the
[Software Grant and Corporate Contributor License Agreement](https://cla.developers.google.com/about/google-corporate).

The following are Google-internal bug numbers where explicit permission from
some authors is recorded for use of their work. (This is purely for our own
record keeping.)
*  27287199
*  27287880
*  27287883
*  263291445
