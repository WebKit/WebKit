There are a couple bug trackers relevant to WebRTC:

  * [crbug.com](https://crbug.com) -- for Chrome.

  * [bugzilla.mozilla.org](https://bugzilla.mozilla.org/) -- for Firefox.

  * [bugs.webkit.org](https://bugs.webkit.org/) -- for Safari.

  * [developer.microsoft.com](https://developer.microsoft.com/en-us/microsoft-edge/platform/issues/) -- for Microsoft Edge.

  * [bugs.opera.com/wizard](https://bugs.opera.com/wizard/) -- for Opera.

  * [bugs.webrtc.org](http://bugs.webrtc.org) -- for WebRTC native code.

Anyone with a [Google account][1] can file bugs in the Chrome and WebRTC trackers and they're continuously triaged by Chrome and WebRTC engineers.


### How to File a Good Bug Report

#### Instructions

* Identify which bug tracker to use:

  * If you're hitting a problem in Chrome, file the bug using the
    [the Chromium issue wizard](https://chromiumbugs.appspot.com/?token=0)
    Choose "Web Developer" and "API", then fill out the form. For the component choose
    * Blink>GetUserMedia for camera/microphone issues
    * Blink>MediaRecording for issues with the MediaRecorder API
    * Blink>WebRTC for issues with the RTCPeerConnection API
    This ensures the right people will look at your bug.

  * If you're a developer working with the native code, file the bug at
    [this link][4].

* Include as much as possible from the data points listed below.

#### Example Data Points

  * Version of the browser/app

    * For Chrome: copy/paste from **chrome://version**

    * For WebRTC native code: if applicable, include the branch (e.g. trunk)
      and WebRTC revision (e.g. r8207) your application uses

  * Operating system (Windows, Mac, Linux, Android, iOS, etc.) and version
    (e.g. Windows 7, OS X 10.9, Ubuntu 14, etc.)

  * Hardware platform/device model (e.g. PC, Mac, Samsung 4S, Nexus 7, iPhone
    5S, iPad Air 2 etc)

  * Camera and microphone model and version (if applicable)

    * For Chrome audio and video device issues, please run the tests at
      <https://test.webrtc.org>. After the tests finish running, click the bug
      icon at the top, download the report, and attach the report to the issue
      tracker.

  * Web site URL

  * Reproduction steps: detailed information on how to reproduce the bug. If
    applicable, please either attach or link to a minimal test page in
    HTML+JavaScript.

  * For **crashes**

    * If you experience a crash while using Chrome, please include a crash ID
      by following [these instructions][2].

    * If you experience a crash while using WebRTC native code, please include
      the full stacktrace.

  * For **functional** issues or **ICE** issues, in either Chrome or a native
    application, please gather a [native log][5].

  * For **connectivity** issues on Chrome, ensure **chrome://webrtc-internals**
    is open in another tab before starting the call and while the call is in progress,

    * expand the **Create Dump** section,

    * click the **Download the PeerConnection updates and stats data** button.
      You will be prompted to save the dump to your local machine. Please
      attach that dump to the bug report.

  * For **audio quality** issues on Chrome, while the call is in progress,

    * please open **chrome://webrtc-internals** in another tab,

    * expand the **Create Dump** section,

    * fill in the **Enable diagnostic audio recordings** checkbox. You will be
      prompted to save the recording to your local machine. After ending the
      call, attach the recording to the bug.

  * For **echo** issues, please try to capture an audio recording from the
    side that is _generating_ the echo, not the side that _hears_ the echo.
    For example, if UserA and UserB are in a call, and UserA hears herself
    speak, please obtain an audio recording from UserB.

  * For **regressions**, i.e. things that worked in one version and stopped working in
    a later versioņ, provide both versions. If you know steps to reproduce you might
    want to try [a bisect](https://www.chromium.org/developers/bisect-builds-py) to
    identify the commit that changed the behaviour.

  * For **video problems**, e.g. artifacts or decoder failures, a rtpdump file
    with the unencrypted RTP traffic. This can by replayed using the video_replay
    tool from the rtc_tools directory.

  * For problem with the webcam, a dump or screenshot of the "Video Capture" tab
    in chrome://media-internals.

### Filing a Security Bug

The WebRTC team takes security very seriously. If you find a vulnerability in
WebRTC, please file a [Chromium security bug][ChromeSecurity], even if the bug
only affects native WebRTC code and not Chromium.

A history of fixed Chromium security bugs is best found via [security notes in
Stable Channel updates on the Google Chrome releases blog][ChromeSecurityBlog].

You can also find fixed, publicly visible [Type=Bug-Security][ChromeBugList]
bugs in the issue tracker (note: security bugs normally become publicly
visible 14 weeks after they are fixed). If there is a bug in WebRTC code
that Chromium isn’t using (such as the Java/ObjC wrappers for Android/iOS)
we will announce fixes separately on [discuss-webrtc][DiscussWebRTC].

[Tracking released security bug disclosures][WebRtcBugList].

Note that we will generally NOT merge security fixes backwards to any branches,
so if you’re using older branches it’s your responsibility to make sure the
relevant security fixes get merged.


### Receiving notifications about security bugs in Chrome

To get automatic notifications about activity/comments in security bugs in
Chrome you need to be either explicitly cc:d on specific bugs (by someone who
has access to the bug) or be part of a special mailing list for all security bug
notifications. To get on that list you have to apply to the Chrome Security
team, see more about this on the [Chrome Security page][ChromeSecurity] under
"How can I get access to Chromium vulnerabilities?" at the bottom of the page.

Please note that Chrome's security-notify list will receive notifications about
all security bugs in Chrome and not just the WebRTC ones. Normally it shouldn't
be a problem to figure out whether an issue affects WebRTC since it will most
likely be tagged with one of the WebRTC-related components (one of Blink>WebRTC,
Blink>GetUserMedia, Blink>MediaStream, Blink>MediaRecording) or their sub-
components.

Also note that access granted by the list will only apply to bugs of Type=Bug-
Security. Not all bugs with crashes, memory leaks and other potential
vulnerabilities are marked as Bug-Security though. You can read more about what
categories of bugs are deemed security bugs in the [Severity Guidelines for
Security Issues][SeverityGuidelines] and also on the [Security FAQ][SecurityFaq]
page.


[1]: https://accounts.google.com/
[2]: http://www.chromium.org/for-testers/bug-reporting-guidelines/reporting-crash-bug
[3]: https://code.google.com/p/chromium/issues/entry?template=Audio/Video%20Issue
[4]: https://bugs.chromium.org/p/webrtc/issues/entry
[5]: native-code/logging.md
[ChromeSecurity]: https://www.chromium.org/Home/chromium-security/reporting-security-bugs
[DiscussWebRTC]: https://groups.google.com/group/discuss-webrtc
[ChromeSecurityBlog]: https://chromereleases.googleblog.com/search/label/Stable%20updates
[ChromeBugList]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=Type%3DBug-Security+component%3ABlink%3EWebRTC+-status%3ADuplicate%2CWontfix&sort=-closed&colspec=ID+Pri+M+Component+Status+Owner+Summary+OS+Closed&x=m&y=releaseblock&cells=ids
[WebRtcBugList]: https://bugs.chromium.org/p/webrtc/issues/list?q=Type%3DBug-Security&can=1
[ChromeSecurity]: https://www.chromium.org/Home/chromium-security
[SeverityGuidelines]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/severity-guidelines.md
[SecurityFaq]: https://chromium.googlesource.com/chromium/src/+/main/docs/security/faq.md
