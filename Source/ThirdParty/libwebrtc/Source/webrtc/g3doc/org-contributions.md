<!-- go/cmark -->
<!--* freshness: {owner: 'hta' reviewed: '2025-10-01'} *-->

# Organizational contributors to WebRTC

This document outlines procedures for the relationship with longer-term
organizational contributors to WebRTC.

Note that this is not covering the case of individual, one-off contributions;
those are adequately covered in other documents.

## Background: Individuals making multiple contributions

The contribution guidelines can be summarized as:

*   First, contribute something to show understanding of the codebase
*   Then, get bot start rights, so that one can test the contributions before
    asking for review (this right applies only to bots that operate on the open
    source repo)
*   After a number of commits, and demonstrating adequate knowledge of the
    project’s style and structure, one can ask for committer rights, which will
    give the ability to submit code after adequate review (current policy:
    review by two WebRTC project members).

## Organizations making multiple contributions

At the moment, primary management of the WebRTC code repository and CI infrastructure is being provided by Google. This means that certain actions require cooperation with the responsible team at Google - here we refer to the people working on this at Google as “the WebRTC project”.

Sometimes, organizations take on a commitment to contribute to WebRTC on a
longer term basis. In these cases, it is good for all parties to have some
guidelines on how the relationship between the WebRTC project and the
organization is managed.

We should have the following roles in place:

*   A contact person at the contributing organization \
    This person will be responsible for knowing where the organization is making
    contributions, and why. All contributors from that organization need to be
    known by that contact person; the WebRTC project may redirect queries from
    other people in the org to that person if not already CCed.
*   At least one person with committer rights (or working towards such rights).
    \
    This person will also be a primary reviewer for incoming CLs from the
    organization, ensuring a review is done before the WebRTC project members
    are asked for review. \
    This can be the same as the contact person, or someone different.

The WebRTC project will offer to host a contact mailing list, if desirable, and name a point of contact for the relationship.

When making small contributions like bug fixes, normal review is sufficient.

When asking to add significant functionality (new CC, new codecs, other new
features), the process should include:

*   Specifying why the feature is needed (requirements, conditions for saying
    “it works”, value to the larger community). This should normally be done
    by filing a bug on the [issues.webrtc.org](https://issues.webrtc.org) bugtracker
    asking for the feature.
*   A design document showing how the feature will be implemented and how it
    will interact with the rest of the WebRTC implementation
*   A plan for who will do the work, and when it’s expected to happen
*   A “match list” of the areas affected by the project and the WebRTC project
    members available to review contributions in those areas. (This can be
    created collaboratively).
*   If the work involves field trials and rollouts on Google properties like
    Meet and Chrome, there
    must be a plan for managing these aspects.

Normally, an ongoing relationship will require some regular cadence of meetings;
a minimum of one hour per quarter should be aimed for, with other meetings as
needed.

