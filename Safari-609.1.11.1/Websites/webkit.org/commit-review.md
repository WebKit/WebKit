The WebKit project has two kinds of special status beyond being a contributor. WebKit Committers have direct read-write access to the Subversion repository, enabling them to commit changes by themselves or reviewed changes by others if authors ask committers to do so. WebKit Reviewers are permitted to review patches and may grant or deny approval for committing. Details of the review and commit process are available on the [contribution page](http://webkit.org/coding/contributing.html).

New WebKit Committers and WebKit Reviewers will be selected by the set of existing WebKit Reviewers. We will create a mailing list, &lt;[webkit-reviewers@lists.webkit.org](mailto:webkit-reviewers@lists.webkit.org)&gt;, for this purpose.

An up to date list of current WebKit Committers and WebKit Reviewers will be maintained at webkit.org.

## Choosing Committers and Reviewers

A candidate for WebKit Committer or WebKit Reviewer should initially be nominated by a reviewer on the reviewers mailing list, in accordance with the criteria below. If the required reviewers (see below) second the nomination, then it carries within 5 business days unless someone objects. If an objection is raised, the reviewers should discuss the matter and try to come to consensus; failing this, the matter will be decided by majority vote of the reviewers.

Once someone is successfully nominated for WebKit Committer status, Apple will take care of sending the committer agreement and setting up a Subversion account once signed and received.

Once someone is successfully nominated for WebKit Reviewer status, the nominating Reviewer or another responsible party should inform the candidate and ask for indication of acceptance from the potential new reviewer. If the candidate accepts, [contributors.json](http://trac.webkit.org/browser/trunk/Tools/Scripts/webkitpy/common/config/contributors.json) will be updated.

## Criteria for Committers

A WebKit Committer should be a person we can trust to follow and understand the project policies about checkins and other matters.

Normally a potential Committer would be nominated once they have submitted around 10-20 good patches, shown good judgment and understanding of project policies, and demonstrated good collaboration skills. To be nominated and seconded, they will have to interact with more than one project reviewer. If someone submits many patches but does not show good judgment or effective collaboration, that contributor might not be nominated right away. If someone submits fewer patches than this but has experience working in another WebKit-based or khtml-based engine and has a track record of good collaboration with the WebKit project, they may be nominated sooner.

Significant contributors to testing, bug management, web site content, documentation, project infrastructure and other non-code areas may also be nominated, even without the normal threshold of patches.

A person who will be working under the supervision of a WebKit reviewer on WebKit-related projects can be nominated if the reviewer is willing to vouch for them and supervise them to ensure they follow project policies on checkins. Supervision does not necessarily imply a manager/employee relationship, just that you work with the potential committer closely enough to make sure they follow policy and work well with others.

All committer nominations require the support of three reviewers. One reviewer nominates, two others second the nomination.

## Criteria for Reviewers

A WebKit Reviewer should be a person who has shown particularly good judgment, understanding of project policies, collaboration skills, and understanding of the code. Reviewers are expected to ensure that patches they review follow project policies, and to do their best to check for bugs or other problems with the patch. They are also expected to show good judgment in whether or not they review a patch at all, or defer to another reviewer.

A potential Reviewer may be nominated once they have submitted a minimum of 80 good patches. They should also be in touch with other reviewers and aware of who are the experts in various areas.

A person who submits many patches but does not show good collaboration skills, code understanding or understanding of project policies may never be nominated. Making unofficial reviews before you become a reviewer is encouraged. This is an excellent way to show your skills. Note that you should not put r+ nor r- on patches in such unofficial reviews.

For Reviewer status, there is no supervision exception.

All reviewer nominations require the support of four reviewers. One reviewer nominates, three reviewers second.

## Inactive Committer or Reviewer Status

A WebKit Committer or Reviewer that has not been active in the project for over a year is considered inactive. Activity for this purpose is defined as landing at least one patch in the past year. Reviewers who have reviewed a patch in the past year will also be considered active.

Inactive Committers can regain Active Committer status by landing (via the Commit Queue) a non-trivial patch and asking on webkit-committers for a return to Active status.

Inactive Reviewers need to show that they are making an effort to get familiar with the changes that have happened in the project since they were last active by landing at least 3 non-trivial patches. Once they have landed the patches, they need to send an email requesting reactivation to webkit-reviewers. This request needs the support of 2 Active Reviewers to be granted.

Note that regardless of a Committer or Reviewer's activity status, any subversion account that has not been used in the past year will be deactivated for security purposes. For example, a Reviewer that has reviewed a patch in the past year but has not committed may have their subversion account deactivated. To reactivate a deactivated subversion account, an Active Committer or Active Reviewer can send an email to webkit-committers requesting it.

## Suspension and Revocation of Committer or Reviewer Status

WebKit Committer or WebKit Reviewer status can be revoked by 2/3 vote of the reviewers, not including the person under consideration for revocation.

Someone actively damaging the repository or intentionally abusing their review privilege may have it temporarily suspended on the request of any two Reviewers. In such a case, the requesting Reviewers should notify the webkit-reviewers list with a description of the offense. At this point, Reviewer or Committer status will be temporarily suspended for one week, pending outcome of the vote for permanent revocation.