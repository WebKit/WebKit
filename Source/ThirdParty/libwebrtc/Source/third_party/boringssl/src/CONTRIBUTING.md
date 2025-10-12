Want to contribute? Great! First, read this page (including the small print at the end).

### Before you contribute
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

### Code reviews
All submissions, including submissions by project members, require review. We
use [Gerrit](https://boringssl-review.googlesource.com) for this purpose.

#### Setup
If you have not done so on this machine, you will need to set up a password for
Gerrit. Sign in with a Google account, visit
[this link](https://boringssl.googlesource.com/), and click the "Generate
Password" link in the top right.

You must also have a Gerrit account associated with
your Google account.  To do this visit the [Gerrit review server](https://boringssl-review.googlesource.com)
and click "Sign in" (top right).

Finally, you will need to prepare your checkout to
[add Change-Ids](https://gerrit-review.googlesource.com/Documentation/cmd-hook-commit-msg.html)
on commit. Run:

    curl -Lo .git/hooks/commit-msg https://boringssl-review.googlesource.com/tools/hooks/commit-msg
    chmod u+x .git/hooks/commit-msg

#### Uploading changes
To upload a change, push it to the special `refs/for/main` target:

    git push origin HEAD:refs/for/main

The output will then give you a link to the change. Add `agl@google.com`,
`davidben@google.com` as reviewers.

Pushing a commit with the same Change-Id as an existing change will upload a new
version of it. (Use the `git rebase` or `git commit --amend` commands.)

For more detailed instructions, see the
[Gerrit User Guide](https://gerrit-review.googlesource.com/Documentation/intro-user.html).

As an alternative to pushing to `refs/for/main`: if you have Chromium's
`depot_tools` installed, you can simply run `git cl upload` to upload a change.
This also has the advantage of automatically running any relevant `PRESUBMIT.py`
checks. See [depot_tools
documentation](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools.html)
for more info.

### Copyright headers
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

### The small print
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
