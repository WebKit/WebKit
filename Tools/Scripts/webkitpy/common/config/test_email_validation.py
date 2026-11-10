# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Apple Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
import json
import sys
import time
import unittest
import urllib.parse
import urllib.request

from webkitpy.common.config.committers import Contributor, CommitterList


class ContributorEmailValidationTest(unittest.TestCase):
    """Tests for Contributor bugzilla_email() validation."""

    def test_single_email_no_validation(self):
        """Test that contributors with a single email don't need validation."""
        contributor = Contributor("Test User", "test@example.com")
        email = contributor.bugzilla_email()
        self.assertEqual(email, "test@example.com")

    def test_email_property_returns_bugzilla_email(self):
        """Test that the email property returns the bugzilla_email()."""
        contributor = Contributor("Test User", "test@example.com")
        self.assertEqual(contributor.email, contributor.bugzilla_email())

    def test_caching_works(self):
        """Test that bugzilla_email() caches the validated email."""
        contributor = Contributor("Test User", ["first@example.com", "second@example.com"])

        # First call
        email1 = contributor.bugzilla_email()

        # Second call should return cached value
        email2 = contributor.bugzilla_email()

        self.assertEqual(email1, email2)
        # Verify it's the same object reference (cached)
        self.assertIs(contributor._bugzilla_email, email1)

    def test_multiple_emails_validation_with_real_data(self):
        """Test email validation with real contributors.json data."""
        committer_list = CommitterList()
        multi_email_contributors = [c for c in committer_list.contributors() if len(c.emails) > 1]

        if not multi_email_contributors:
            self.skipTest("No contributors with multiple emails found")

        # Test first contributor with multiple emails
        contributor = multi_email_contributors[0]
        validated_email = contributor.bugzilla_email()

        # Should return one of the emails
        self.assertIn(validated_email, contributor.emails)

        # Should be cached
        cached_email = contributor.bugzilla_email()
        self.assertEqual(validated_email, cached_email)


def _print_separator():
    print("=" * 70)


def _test_contributor_by_name(contributor_name, debug=False):
    """Test email validation for a specific contributor by name."""
    _print_separator()
    print("Testing email validation for: {}".format(contributor_name))
    _print_separator()

    print("\nLoading contributors from contributors.json...")
    committer_list = CommitterList()

    # Try to find the contributor by name (case-insensitive)
    contributor = committer_list.contributor_by_name(contributor_name)

    if not contributor:
        # Try searching by partial name match
        all_contributors = committer_list.contributors()
        matches = [c for c in all_contributors if contributor_name.lower() in c.full_name.lower()]

        if not matches:
            print("Contributor '{}' not found in contributors.json".format(contributor_name))
            print("\nTip: Try a partial name match, e.g., 'Smith' to find 'John Smith'")
            return False

        if len(matches) > 1:
            print("Multiple contributors found matching '{}':".format(contributor_name))
            for match in matches:
                print("   - {}".format(match.full_name))
            print("\nPlease be more specific.")
            return False

        contributor = matches[0]

    print("Found contributor: {}".format(contributor.full_name))
    print("\nEmail addresses in contributors.json:")
    for i, email in enumerate(contributor.emails):
        print("  [{}] {}".format(i + 1, email))

    if len(contributor.emails) == 1:
        print("\nThis contributor only has one email address.")
        print("  No validation needed, will use: {}".format(contributor.emails[0]))
    else:
        print("\nValidating {} email addresses against Bugzilla...".format(len(contributor.emails)))

    # If debug mode, manually validate and show details
    if debug and len(contributor.emails) > 1:
        print("\n" + "=" * 70)
        print("DEBUG: Manual validation to see what's happening")
        print("=" * 70)

        from webkitpy.common.config import urls

        print("\nTesting each email address individually...")
        print("(Bugzilla REST API only accepts one email at a time)")

        for i, email in enumerate(contributor.emails):
            print("\n[{}] Testing: {}".format(i + 1, email))
            api_url = "{}rest/user?names={}".format(urls.bug_server_url, urllib.parse.quote(email))
            print("    API URL: {}".format(api_url))

            try:
                req = urllib.request.Request(api_url)
                with urllib.request.urlopen(req, timeout=5) as response:
                    response_text = response.read().decode('utf-8')
                    data = json.loads(response_text)

                    print("    Response:")
                    if 'users' in data and len(data['users']) > 0:
                        print("      User found!")
                        for user in data['users']:
                            print("        - name: {}".format(user.get('name')))
                            print("        - id: {}".format(user.get('id')))
                            print("        - real_name: {}".format(user.get('real_name', 'N/A')))
                            if user.get('name', '').lower() == email:
                                print("      Email MATCH: This is the valid email!")
                    else:
                        print("      No user found for this email")
                        print("      Raw response: {}".format(response_text[:200]))

            except Exception as e:
                print("      Error: {}: {}".format(type(e).__name__, e))

        print("\n" + "=" * 70)

    # Validate email (this will use the actual Contributor class method)
    print("\nRunning actual validation via Contributor.bugzilla_email()...")
    start = time.time()
    validated_email = contributor.bugzilla_email()
    elapsed = time.time() - start

    print("\nValidated email: {}".format(validated_email))
    print("  (Validation took {:.3f}s)".format(elapsed))

    # Show if validation picked a different email
    if validated_email != contributor.emails[0]:
        print("\nVALIDATION WORKED!")
        print("   Selected: {}".format(validated_email))
        print("   Instead of first email: {}".format(contributor.emails[0]))
    elif len(contributor.emails) > 1:
        print("\n   First email was valid (or validation failed, using fallback)")

    # Test caching
    print("\nTesting cache...")
    start = time.time()
    cached_email = contributor.bugzilla_email()
    elapsed = time.time() - start
    print("Cached email: {} (took {:.6f}s - should be instant)".format(cached_email, elapsed))

    # Test email property
    property_email = contributor.email
    print("Email property: {}".format(property_email))

    assert validated_email == cached_email == property_email, "All methods should return the same email"

    print("\n" + "=" * 70)
    print("Email validation test completed successfully!")
    print("=" * 70)
    return True


def _test_with_real_data():
    """Test email validation with real contributors.json data."""
    _print_separator()
    print("Testing email validation with REAL contributors.json data")
    _print_separator()

    print("\nLoading contributors from contributors.json...")
    committer_list = CommitterList()
    all_contributors = committer_list.contributors()
    print("Loaded {} contributors".format(len(all_contributors)))

    # Find contributors with multiple emails
    multi_email_contributors = [c for c in all_contributors if len(c.emails) > 1]
    print("Found {} contributors with multiple emails".format(len(multi_email_contributors)))

    if not multi_email_contributors:
        print("No contributors with multiple emails found. Skipping real data tests.")
        return

    # Test the first few contributors with multiple emails
    print("\n" + "=" * 70)
    print("Testing email validation for contributors with multiple emails:")
    print("=" * 70)

    for i, contributor in enumerate(multi_email_contributors[:5]):  # Test first 5
        print("\n[{}] {}".format(i + 1, contributor.full_name))
        print("    All emails: {}".format(contributor.emails))

        start = time.time()
        validated_email = contributor.bugzilla_email()
        elapsed = time.time() - start

        print("    Validated email: {}".format(validated_email))
        print("    (Validation took {:.3f}s)".format(elapsed))

        # Test caching
        start = time.time()
        cached_email = contributor.bugzilla_email()
        elapsed = time.time() - start
        print("    Cached email: {} (took {:.6f}s)".format(cached_email, elapsed))

        assert validated_email == cached_email, "Cached email should match validated email"

        if validated_email != contributor.emails[0]:
            print("    VALIDATION WORKED! Selected '{}' instead of '{}'".format(
                validated_email, contributor.emails[0]))

    print("\n" + "=" * 70)
    print("All real data tests passed!")
    print("=" * 70)


def _test_basic_functionality():
    """Test basic functionality with mock data."""
    print("\n" + "=" * 70)
    print("Testing basic functionality")
    print("=" * 70)

    # Test 1: Contributor with single email
    print("\nTest 1: Single email (should not validate)")
    contributor1 = Contributor("Test User", "test@example.com")
    email1 = contributor1.bugzilla_email()
    print("  Result: {}".format(email1))
    assert email1 == "test@example.com", "Single email should be returned as-is"
    print("  Passed")

    # Test 2: email property works
    print("\nTest 2: email property returns correct value")
    email2 = contributor1.email
    print("  Result: {}".format(email2))
    assert email2 == "test@example.com", "email property should return bugzilla_email()"
    print("  Passed")

    print("\nAll basic tests passed!")


def main():
    parser = argparse.ArgumentParser(
        description='Test email validation for WebKit contributors',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  python3 -m webkitpy.common.config.test_email_validation "Foo Bar"
  python3 -m webkitpy.common.config.test_email_validation "Foo Bar" --debug
  python3 -m webkitpy.common.config.test_email_validation --all
  python3 -m webkitpy.common.config.test_email_validation --unittest
        '''
    )
    parser.add_argument(
        'name',
        nargs='?',
        help='Contributor name to test (full or partial name)'
    )
    parser.add_argument(
        '--all',
        action='store_true',
        help='Run tests on first 5 contributors with multiple emails'
    )
    parser.add_argument(
        '--debug',
        action='store_true',
        help='Show detailed debug information including API calls and responses'
    )
    parser.add_argument(
        '--unittest',
        action='store_true',
        help='Run unit tests'
    )

    args = parser.parse_args()

    if args.unittest:
        # Run unit tests
        suite = unittest.TestLoader().loadTestsFromTestCase(ContributorEmailValidationTest)
        runner = unittest.TextTestRunner(verbosity=2)
        result = runner.run(suite)
        sys.exit(0 if result.wasSuccessful() else 1)
    elif args.name:
        # Test specific contributor
        _test_contributor_by_name(args.name, debug=args.debug)
    elif args.all:
        # Run all tests
        _test_basic_functionality()
        _test_with_real_data()
    else:
        # No arguments, show help
        parser.print_help()
        print("\n" + "=" * 70)
        print("Quick start:")
        print("  python3 -m webkitpy.common.config.test_email_validation 'Your Name'")
        print("  python3 -m webkitpy.common.config.test_email_validation 'Your Name' --debug")
        print("  python3 -m webkitpy.common.config.test_email_validation --all")
        print("  python3 -m webkitpy.common.config.test_email_validation --unittest")
        print("=" * 70)


if __name__ == "__main__":
    main()
