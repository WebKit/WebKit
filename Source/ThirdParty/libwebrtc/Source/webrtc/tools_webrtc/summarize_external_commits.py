#!/usr/bin/env python3
# Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.
"""Script to summarize external contributors to the libwebrtc codebase. """

import subprocess
from collections import defaultdict
import operator
from dataclasses import dataclass


@dataclass
class CommitSummaries:
    """Holds various summaries of commits."""
    author_counts: defaultdict
    domain_counts: defaultdict
    domain_non_hancke_counts: defaultdict
    monthly_summary: defaultdict
    monthly_non_hancke_summary: defaultdict
    monthly_all_summary: defaultdict


def get_external_commits():
    # Define corporate domains and service account patterns to exclude
    corporate_domains = ('chromium.org', 'webrtc.org', 'google.com',
                         'gserviceaccount.com')

    # Use origin/main as the reference for the main branch
    # We use --since="3 years ago" and format to get author email
    # and commit date
    cmd = [
        'git', 'log', 'origin/main', '--since="3 years ago"',
        '--format=%ae %cs'
    ]

    try:
        output = subprocess.check_output(
            cmd, stderr=subprocess.STDOUT).decode('utf-8')
    except subprocess.CalledProcessError as err:
        print(f"Error running git log: {err.output.decode('utf-8')}")
        return CommitSummaries(defaultdict(int), defaultdict(int),
                               defaultdict(int), defaultdict(int),
                               defaultdict(int), defaultdict(int))

    monthly_summary = defaultdict(int)
    monthly_non_hancke_summary = defaultdict(int)
    monthly_all_summary = defaultdict(int)
    author_counts = defaultdict(int)
    domain_counts = defaultdict(int)
    domain_non_hancke_counts = defaultdict(int)

    for line in output.strip().split('\n'):
        if not line:
            continue
        parts = line.split(' ')
        if len(parts) < 2:
            continue
        email = parts[0].lower()  # Normalize to lowercase
        date_str = parts[1]
        month_key = date_str[:7]

        # Exclude common bot/service account prefixes from all totals
        if email.startswith('webrtc-version-updater') or email.startswith(
                'chromium-webrtc-autoroll'):
            continue

        # Track all commits per month (before filtering)
        monthly_all_summary[month_key] += 1

        # Check if email domain is corporate or a service account
        domain = email.split('@')[-1] if '@' in email else ''
        if any(domain == d or domain.endswith('.' + d)
               for d in corporate_domains):
            continue

        # Update author counts
        author_counts[email] += 1
        domain_counts[domain] += 1

        # Convert date_str (YYYY-MM-DD) to YYYY-MM for monthly summary
        monthly_summary[month_key] += 1

        # Track commits not authored by "hancke"
        if 'hancke' not in email:
            monthly_non_hancke_summary[month_key] += 1
            domain_non_hancke_counts[domain] += 1

    return CommitSummaries(author_counts, domain_counts,
                           domain_non_hancke_counts, monthly_summary,
                           monthly_non_hancke_summary, monthly_all_summary)


def main():
    summaries = get_external_commits()
    author_counts = summaries.author_counts
    domain_counts = summaries.domain_counts
    domain_non_hancke_counts = summaries.domain_non_hancke_counts
    monthly_summary = summaries.monthly_summary
    monthly_non_hancke_summary = summaries.monthly_non_hancke_summary
    monthly_all_summary = summaries.monthly_all_summary

    if not author_counts:
        print("No external commits found in the last 3 years.")
        return

    # Sort authors by commit count descending
    top_20_authors = sorted(author_counts.items(),
                            key=operator.itemgetter(1),
                            reverse=True)[:20]

    print("Top 20 External Committers (Last 3 Years):")
    print(f"{'Author Email':<40} | {'Commits':<8}")
    print("-" * 52)
    for email, count in top_20_authors:
        print(f"{email:<40} | {count:<8}")

    # Sort domains by commit count descending
    top_20_domains = sorted(domain_counts.items(),
                            key=operator.itemgetter(1),
                            reverse=True)[:20]

    print("\nTop 20 External Domains (Last 3 Years):")
    print(f"{'Domain':<40} | {'External':<10} | {'Non-Hancke':<12}")
    print("-" * 68)
    for domain, count in top_20_domains:
        non_hancke = domain_non_hancke_counts.get(domain, 0)
        print(f"{domain:<40} | {count:<10} | {non_hancke:<12}")

    print("\nMonthly Summary of Commits:")
    print(f"{'Month':<10} | {'All':<8} | {'External':<8} | {'Non-Hancke':<12}")
    print("-" * 49)
    # We use monthly_all_summary keys to ensure we cover all months with
    # activity
    for month in sorted(monthly_all_summary.keys(), reverse=True):
        all_commits = monthly_all_summary[month]
        external = monthly_summary.get(month, 0)
        non_hancke = monthly_non_hancke_summary.get(month, 0)
        print(f"{month:<10} | {all_commits:<8} | {external:<8} | "
              f"{non_hancke:<12}")


if __name__ == "__main__":
    main()
