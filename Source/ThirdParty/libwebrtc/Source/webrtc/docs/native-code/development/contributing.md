# Contributing to the WebRTC project

## License Agreement

WebRTC welcomes patches for features and bug fixes!

For contributors external to Google, follow the instructions given in the
[Google Individual Contributor License Agreement][Google individual CLA].
In all cases, contributors must sign a contributor license agreement before
a contribution can be accepted. Please complete the agreement for an
[individual][individual] or a [corporation][corporation] as appropriate.

[Google Individual CLA]: https://cla.developers.google.com/about/google-individual.
[individual]: https://developers.google.com/open-source/cla/individual
[corporation]: https://developers.google.com/open-source/cla/corporate


## Instructions

### Contributing your First Patch
You must do some preparation in order to upload your first CL:

* [Check out and build the code][check out and build the code]
* Fill in the Contributor agreement (see above)
* If you’ve never submitted code before, you must add your
  (or your organization’s in the case the contributor agreement is signed by
  your organization) name and contact info to the
 [AUTHORS][AUTHORS] file
* Go to [https://webrtc.googlesource.com/new-password](new-password)
  and login with your email account. This should be the same account as
  returned by `git config user.email`
* Then, run: `git cl creds-check`. If you get any errors, ask for help on
 [discuss-webrtc][discuss-webrtc]

You will not have to repeat the above. After all that, you’re ready to upload:

[Check out and the build code]: https://webrtc.googlesource.com/src/+/refs/heads/main/docs/native-code/development/index.md
[AUTHORS]: https://webrtc.googlesource.com/src/+/refs/heads/main/AUTHORS
[new-password]: https://webrtc.googlesource.com/new-password
[discuss-webrtc]: https://groups.google.com/forum/#!forum/discuss-webrtc

### Uploading your First Patch
Now that you have your account set up, you can do the actual upload:

*  Do this:
    * Assuming you're on the main branch:
        * `git checkout -b my-work-branch`
    * Make changes, build locally, run tests locally
        * `git commit -am "Changed x, and it is working"`
        * `git cl upload`

      This will open a text editor showing all local commit messages, allowing you
      to modify it before it becomes the CL description.

      Fill out the bug entry properly. Please specify the issue tracker prefix and
      the issue number, separated by a colon, e.g. `webrtc:123` or `chromium:12345`.
      If you do not have an issue tracker prefix and an issue number just add `None`.

      Save and close the file to proceed with the upload to the WebRTC
      [code review server](https://webrtc-review.googlesource.com/q/status:open).

      The command will print a link like
      [https://webrtc-review.googlesource.com/c/src/+/53121][example CL link].
      if everything goes well.

*  Click this CL Link
*  If you’re not signed in, click the Sign In button in the top right and sign
   in with your email
*  Click Start Review and add a reviewer. You can find reviewers in OWNERS files
   around the repository (take the one closest to your changes)
*  Address any reviewer feedback:
    * Make changes, build locally, run tests locally
        * `git commit -am "Fixed X and Y"`
        * `git cl upload`
*  Once the reviewer LGTMs (approves) the patch, ask them to put it into the
   commit queue

NOTICE: On Windows, you’ll need to run the above in a Git bash shell in order
for gclient to find the `.gitcookies` file.

[example CL link]: https://webrtc-review.googlesource.com/c/src/+/53121

### Trybots

If you're working a lot in WebRTC, you can apply for *try rights*. This means you
can run the *trybots*, which run all the tests on all platforms. To do this,
file a bug using this [template][template-access] and the WebRTC EngProd team
will review your request.

To run a tryjob, upload a CL as described above and click either CQ dry run or
Choose Trybots in the Gerrit UI. You need to have try rights for this. Otherwise,
ask your reviewer to kick off the bots for you.

If you encounter any issues with the bots (flakiness, failing unrelated to your change etc),
please file a bug using this [template][template-issue].

[template-access]: https://bugs.chromium.org/p/webrtc/issues/entry?template=Get+tryjob+access
[template-issue]: https://bugs.chromium.org/p/webrtc/issues/entry?template=trybot+issue

