### How To Report Security Bugs

1. **Reporting an issue:** Start by filing a bug in the Security product in the WebKit bug database, at [https://bugs.webkit.org](https://bugs.webkit.org). Bugs in the Security product will have special access controls that restrict who can view and alter the bug; only members of the WebKit Security Group and the originator will have access to the bug.
2. **Scope of disclosure:** If you would like to limit further dissemination of the information in the bug report, please say so in the bug. Otherwise the WebKit Security Group may share information with other vendors if we find they may be affected by the same vulnerability. The WebKit Security Group will handle the information you provide responsibly. See the other sections of this document for details.
3. **Getting feedback:** We cannot guarantee a prompt human response to every security bug filed. If you would like immediate feedback on a security issue, or would like to discuss details with members of the WebKit Security Group, please email [security@webkit.org](mailto:security@webkit.org) and include a link to the relevant Bugzilla bug. Your message will be acknowledged within a week at most.

The current member list is published on the [Security Team](/security-team) page.

### How To Join the WebKit Security Group

1. **Criteria:** Nominees for WebKit Security Group membership should meet at least one of the following criteria:
    - **Individuals:**
        - The nominee specializes in fixing WebKit security related bugs or often participates in their exploration and resolution.
        - The nominee has a track record of finding security vulnerabilities and responsible disclosure of those vulnerabilities.
        - The nominee is a web technology expert who has specific interests in knowing about, resolving, and preventing future security vulnerabilities.
    - **Vendor contacts:**
        - The nominee represents an organization or company which ships products that include their own copy of WebKit. Due to their position in the organization, the nominee has a reasonable need to know about security issues and disclosure embargoes.
    
2. **Nomination process:** Anyone who feels they meet these criteria can nominate themselves by mailing [security@webkit.org](mailto:security@webkit.org), or may be nominated by a third party such as an existing WebKit Security Group member. The nomination email should state whether the nominee is nominated as an individual or as a vendor contact and clearly describe the grounds for nomination.
3. **Choosing new members:** If a nomination for Security Group membership is supported by at least three existing Security Group members (either one initial nomination and two seconds, or in the case of self-nomination, three seconds), then it carries within 5 business days unless an existing member of the Security Group objects. If an objection is raised, the WebKit Security Group members should discuss the matter and try to come to consensus; failing this, the nomination will succeed only by majority vote of the WebKit Security Group. After a vote is called for on the mailing list, voting will be open for 5 business days.
4. **Accepting membership:** Before new WebKit Security Group membership is finalized, the successful nominee should accept membership and agree to abide by this security policy, particularly Privileges and Responsibilities of WebKit Security Group members.
5. **Duration of membership:** Vendor contacts will only remain members as long as their position with that vendor remains the same. Individuals will remain members indefinitely until they resign or their membership is terminated.

### Privileges and Responsibilities of WebKit Security Group Members

- **Access:** WebKit Security Group members will be subscribed to a private mailing list, [security@webkit.org](mailto:security@webkit.org). It will be used for technical discussions of security bugs, as well as process discussions about matters such as disclosure timelines and group membership. Members will also have access to all bugs in the Security product in the WebKit bug database.
- **Confidentiality:** Members of the WebKit Security Group will be expected to treat WebKit security vulnerability information shared with the group as confidential until publicly disclosed:
    - Members should not disclose Security bug information to non-members unless the member is employed by the vendor of a WebKit based product, in which case information can be shared within that organization on a need-to-know basis and handled as confidential information normally is within that organization. The one exception to this rule is that members may share vulnerabilities with vendors of non-WebKit based products if their product suffers from the same issue and the reporter has not explicitly requested this not be done. The non-WebKit vendor should be asked to respect the issue's embargo date, and to not share the information beyond the need-to-know people within their organization.
    - Members should not post any information about Security bugs in public forums.
- **Disclosure:** The WebKit Security Group will negotiate an embargo date for public disclosure for each new Security bug, with a default minimum time limit of 60 days. An embargo may be lifted before the agreed-upon date if all vendors planning to ship a fix have already done so, and if the reporter does not object. The agreed-upon embargo date will be communicated to the reporter through the bug at [https://bugs.webkit.org](https://bugs.webkit.org).
- **Collaboration:** Members of the WebKit Security Group are expected to promptly share any WebKit vulnerabilities they become aware of. The best way to do this is by filing bugs against the Security product in the WebKit bug database.

### Termination of WebKit Security Group Membership

- Members of the WebKit Security Group may voluntarily end their membership at any time, for any reason.
- Inactive members who are no longer reachable via e-mail at the address associated with their group membership will be removed from the WebKit Security Group.
- A member who joined the group as a vendor contact who is no longer associated with that vendor will be removed from the WebKit Security Group. The person may be re-nominated as an individual expert or as a vendor contact for another organization.
- If a member of the WebKit Security Group does not act in accordance with the letter and spirit of this policy, then their WebKit Security Group membership can be revoked by a majority vote of the members, not including the person under consideration for revocation. After a member calls for a revocation vote on the mailing list, voting will be open for 5 business days.
    - **Emergency suspension:** A WebKit Security Group member who blatantly disregards the WebKit Security Policy may have their membership temporarily suspended on the request of any two members. In such a case, the requesting members should notify the security mailing list with a description of the offense. At this point, membership will be temporarily suspended for one week, pending outcome of the vote for permanent revocation.

### Changes to the Policy

The WebKit Security Policy may be changed in the future by rough consensus of the WebKit Security Group. Changes to the policy will be posted publicly.