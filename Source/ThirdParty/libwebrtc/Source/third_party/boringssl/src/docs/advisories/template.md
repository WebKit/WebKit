# Template for OpenSSL Advisories:

Instructions:

1. Copy this file to `YYYY-MM-DD.md` in directory.
2. Fill in the title and table.
3. If BoringSSL is not affected by any issue in the advisory add "(BoringSSL Not Affected)" to the title.
4. Add sections to the end for any issues that warrant extended discussion, e.g. if the severity differs from OpenSSL, or if we have anything to add to OpenSSL's advisory.
5. Delete this header.

---

# OpenSSL Advisory: ${Month} ${Day}, ${Year}

OpenSSL have published a [security advisory](LINK). Here's how it affects BoringSSL:

CVE | Summary | [Severity] in OpenSSL | Impact to BoringSSL
----|---------|-----------------------|---------------------
CVE-YYYY-NNNN | Buffer overflow printing favorite color | High | Not affected, impacted code was removed from BoringSSL in the initial fork
CVE-YYYY-NNNN | Library misreports favorite color | Moderate | Not affected, issue was introduced after fork
CVE-YYYY-NNNN | Timing side channel in favorite color calculation | Low | Affected. Fixed in ...
CVE-YYYY-NNNN | Null pointer dereference when enumerating colors | Low | See discussion below. Fixed in ...

[Severity]: https://openssl-library.org/policies/general/security-policy/index.html#issue-severity

## CVE-YYYY-NNNN

If we need to write a lot about an issue, put it in a section like this.
