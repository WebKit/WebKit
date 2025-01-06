<!-- go/cmark -->
<!--* freshness: {owner: 'titovartem' reviewed: '2024-09-09'} *-->

# How to get tryjob access or become WebRTC committer

## Overview

There are two levels of WebRTC contributors access:

1.  Tryjob access - permits contributor to run tests for their changes using
    WebRTC infrastructure
2.  WebRTC committer rights - permits to submit changes to the WebRTC code base.
    This includes tryjob access.

## Getting tryjob access

To get tryjob access applicant has to do a contribution around 10-20 CLs to the
WebRTC code base. After that, they should file a bug using
[Get tryjob access template][7], specifying the email which was used for the
contributions and to which access will be granted, and listing contributed CLs.

The access will be granted when the ticket is resolved by one of the project
members. In case of rejection the explanation will be provided.

## WebRTC committer duties

WebRTC committers are responsible for keeping WebRTC codebase in a good shape
including, but not limited to the following aspects:

*   Code complexity and correctness
*   C++ best practices
*   Code formatting
*   Test coverage
*   Class/function level and conceptual documentation

Whenever a committer sets `Code Review +1` label on the CL, they approve that
the CL fulfills WebRTC style guides, language mastery, testability and
documentation. Being a committer means being responsible for the WebRTC codebase
health and code quality.

## Becoming a WebRTC committer

To write code in WebRTC you don't need to be a committer (also see [FAQ][1]),
but to submit code to WebRTC you do. So if you don't plan to work on the WebRTC
codebase regularly, you can ask other committers through code review to submit
your patches, but if you are going to work in the WebRTC codebase, then it's
recommended to apply for WebRTC committer rights obtaining process.

1.  If you are going to write in C++ make yourself familiar with with C++ style
    guides:

    *   [Google style guide][5]
    *   [Chromium style guide][2]
    *   [WebRTC style guide][3]

2.  Create a ticket to obtain WebRTC committers rights in Monorail. Please use
    [this template][6] of it.

3.  Pick a mentor among WebRTC committers, who will review your CLs. For C++
    authors, the mentor will also look for C++ readability skills. It's
    recommended to ask someone who is familiar with the code base which you will
    be working on (you can check OWNERS files to find such person). Otherwise
    you can reach out to committers mailing list \<committers@webrtc.org\>.

4.  Send CLs to the mentor for review and attach them to the created ticket.

5.  When the mentor decides that you are ready (for C++ authors their C++
    readability skills are good enough), they will send a proposal for granting
    WebRTC committer rights to the reviewing committee mailing list to review.
    If the proposal will be approved, then committer rights will be granted.
    Committee members will have up to 5 business days to answer. In case of
    rejection detailed feedback on what aspects should be improved will be
    provided.

6.  Also as any contributor you must sign and return the
    [Contributor License Agreement][4]

[1]: https://webrtc.googlesource.com/src/+/refs/heads/main/docs/faq.md#to-be-a-contributor_do-i-need-to-sign-any-agreements
[2]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/styleguide/c++/c++.md
[3]: https://webrtc.googlesource.com/src/+/refs/heads/main/g3doc/style-guide.md
[4]: https://developers.google.com/open-source/cla/individual?hl=en
[5]: https://google.github.io/styleguide/cppguide.html
[6]: https://bugs.chromium.org/p/webrtc/issues/entry?template=Become+WebRTC+committer
[7]: https://bugs.chromium.org/p/webrtc/issues/entry?template=Get+tryjob+access
