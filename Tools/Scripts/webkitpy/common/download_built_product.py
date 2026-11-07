# Copyright (C) 2026 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Download and extract a built product that build.webkit.org uploaded.

The builders of build.webkit.org that have "triggers" in their configuration
archive what they build and upload it to S3, under
"{platform}-{architectures}-{configuration}/{revision}.zip". That directory name
is what this script calls a configuration, for example "wpe-x86_64-release" or
"mac-tahoe-x86_64-arm64-debug". The list of configurations, and the builder
behind each one, are read from Tools/CISupport/build-webkit-org/config-template.yaml,
rendered with webkitcorepy's Config the same way build.webkit.org renders it
into its config.json, so they follow the bots without being written down
anywhere else.

Downloading and extracting call the same tools, with the same arguments, as
the bots that test those products (DownloadBuiltProduct and ExtractBuiltProduct
in build-webkit-org/steps.py).
"""

import argparse
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from itertools import islice

from webkitcorepy import Config

TOP_LEVEL_DIRECTORY = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
# The source of build.webkit.org's config.json, which is generated from it (see render_config() in build-webkit-org/loadConfig.py).
CONFIG_TEMPLATE_PATH = os.path.join(TOP_LEVEL_DIRECTORY, 'Tools', 'CISupport', 'build-webkit-org', 'config-template.yaml')
DOWNLOAD_BUILT_PRODUCT = os.path.join(TOP_LEVEL_DIRECTORY, 'Tools', 'CISupport', 'download-built-product')
BUILT_PRODUCT_ARCHIVE = os.path.join(TOP_LEVEL_DIRECTORY, 'Tools', 'CISupport', 'built-product-archive')

BUILDBOT_URL = 'https://build.webkit.org'
ARCHIVES_URL = 'https://s3-us-west-2.amazonaws.com/archives.webkit.org'

# Buildbot results that mean the build finished its steps, the upload included.
SUCCESSFUL_RESULTS = (0, 1)  # SUCCESS, WARNINGS
BUILDS_PER_REQUEST = 100
# How far back to look for one revision, so that a revision older than the builder's history ends with an error instead of paging forever.
MAX_BUILDS_TO_SEARCH = 2000
# Builds can finish out of revision order (parallel workers, forced builds of older revisions), so a search goes on for this many products
# past the first one older than the revision it looks for.
PRODUCTS_TO_SEARCH_PAST_AN_OLDER_ONE = 20
DEFAULT_LIST_LIMIT = 20

IDENTIFIER_RE = re.compile(r'^(\d+)(@main)?$')
# How the products of release branches (e.g. webkitglib/2.52) are named: "{branch point}.{commit on the branch}@{branch}".
BRANCH_IDENTIFIER_RE = re.compile(r'^\d+\.\d+@\S+$')
# The revision argument that asks for the newest product.
LAST = 'last'


class Configuration(object):
    """A directory of built products on S3, and the builder that uploads to it."""

    def __init__(self, platform, architectures, configuration, builder):
        self.platform = platform
        self.architectures = architectures
        self.configuration = configuration
        self.builder = builder

    @property
    def name(self):
        # Same as GenerateS3URL in build-webkit-org/steps.py: "{fullPlatform}-{archForUpload}-{configuration}".
        architectures = '-'.join(self.architectures)
        return f'{self.platform}-{architectures}-{self.configuration}'

    @property
    def generic_platform(self):
        # The "platform" build property, which DownloadBuiltProduct passes.
        if self.platform == 'jsc-only':
            return self.platform
        return self.platform.split('-', 1)[0]

    def archive_url(self, archive_revision):
        return f'{ARCHIVES_URL}/{self.name}/{archive_revision}.zip'

    def builder_url(self):
        return f'{BUILDBOT_URL}/#/builders/{self.builder}'

    def download_command(self, archive_revision):
        return [sys.executable, DOWNLOAD_BUILT_PRODUCT, f'--platform={self.generic_platform}', f'--{self.configuration}', self.archive_url(archive_revision)]

    def extract_command(self):
        return [sys.executable, BUILT_PRODUCT_ARCHIVE, f'--platform={self.platform}', f'--{self.configuration}', 'extract']


class Product(object):
    """One uploaded archive: a successful build of a configuration's builder."""

    def __init__(self, archive_revision, git_hash, build_number, completed_at):
        self.archive_revision = archive_revision
        self.git_hash = git_hash
        self.build_number = build_number
        self.completed_at = completed_at

    @property
    def identifier(self):
        match = IDENTIFIER_RE.match(self.archive_revision or '')
        return int(match.group(1)) if match and match.group(2) else None

    def describe(self):
        when = datetime.fromtimestamp(self.completed_at, timezone.utc).strftime('%Y-%m-%d %H:%MZ') if self.completed_at else '?'
        short_hash = (self.git_hash or '')[:12]
        return f'{self.archive_revision:<13} {short_hash:<12} {when}  (build {self.build_number})'


def load_configurations(config):
    """Map each configuration name to its Configuration, from the rendered build.webkit.org configuration."""
    configurations = {}
    for builder in config.get('builders', []):
        if not builder.get('triggers'):
            continue
        configuration = Configuration(builder['platform'], builder['architectures'], builder['configuration'], builder['name'])
        configurations[configuration.name] = configuration
    return configurations


def read_config(path=CONFIG_TEMPLATE_PATH):
    with open(path) as config_file:
        return Config.load(config_file)


def fetch_json(url):
    with urllib.request.urlopen(url, timeout=60) as response:
        return json.loads(response.read().decode('utf-8'))


def products(configuration, fetch=fetch_json, max_builds=MAX_BUILDS_TO_SEARCH):
    """Yield the configuration's products, newest first."""
    seen = 0
    while seen < max_builds:
        builder = urllib.parse.quote(configuration.builder, safe='')
        url = f'{BUILDBOT_URL}/api/v2/builders/{builder}/builds?order=-number&limit={BUILDS_PER_REQUEST}&offset={seen}&complete=true&property=archive_revision&property=got_revision&property=user_provided_git_hash'
        builds = fetch(url).get('builds', [])
        if not builds:
            return
        seen += len(builds)
        for build in builds:
            if build.get('results') not in SUCCESSFUL_RESULTS:
                continue
            properties = build.get('properties', {})
            archive_revision = (properties.get('archive_revision') or [None])[0]
            if not archive_revision:
                continue
            # A build forced with a hash checks that hash out after got_revision, and archives it (see ShowIdentifier in build-webkit-org/steps.py).
            git_hash = (properties.get('user_provided_git_hash') or [None])[0] or (properties.get('got_revision') or [None])[0]
            yield Product(archive_revision, git_hash, build.get('number'), build.get('complete_at'))


def same_commit(hash, other_hash):
    # The hash of a forced build is typed by hand, so it may be abbreviated.
    return bool(hash and other_hash) and (hash.startswith(other_hash) or other_hash.startswith(hash))


def find_product(candidates, identifier, git_hash=None, name=None):
    """Return (product, older, newer): the product named name, or for the commit git_hash, or for the main identifier, or else the nearest products on each side of the identifier."""
    older, newer = None, None
    past_older = 0
    for product in candidates:
        if older:
            past_older += 1
            if past_older > PRODUCTS_TO_SEARCH_PAST_AN_OLDER_ONE:
                break
        if (name and product.archive_revision == name) or same_commit(git_hash, product.git_hash) or (identifier is not None and product.identifier == identifier):
            return product, None, None
        if product.identifier is None or identifier is None:
            continue
        if product.identifier > identifier:
            if not newer or product.identifier < newer.identifier:
                newer = product
        elif not older or product.identifier > older.identifier:
            older = product
    return None, older, newer


def resolve_revision(revision, repository_path=TOP_LEVEL_DIRECTORY):
    """Return (identifier, git_hash, name, note) for a revision argument: N, N@main, N.M@branch, a git hash or ref, or None for the checked-out commit.

    name is how the product of the commit would be named. identifier is the main identifier, or for a commit on another branch, the identifier
    of the main commit it is based on, whose product is used when the branch has no products of its own (as with local development branches).
    """
    match = IDENTIFIER_RE.match(revision or '')
    if match:
        return int(match.group(1)), None, f'{match.group(1)}@main', None
    if BRANCH_IDENTIFIER_RE.match(revision or ''):
        try:
            return resolve_commit(revision, repository_path)
        except Exception:
            # The branch is not in the checkout, as with a release branch that was never checked out: look for a product with that name.
            return None, None, revision, None
    return resolve_commit(revision, repository_path)


def resolve_commit(revision, repository_path):
    from webkitscmpy import local
    repository = local.Git(repository_path)
    commit = repository.find(revision or 'HEAD', include_log=False)
    if not commit:
        raise ValueError(f"Could not find {revision or 'HEAD'} in {repository_path}")
    if commit.branch == repository.default_branch:
        return commit.identifier, commit.hash, f'{commit.identifier}@main', None
    if not commit.branch_point:
        raise ValueError(f'{commit} is not on {repository.default_branch}, so it has no built product')
    base = repository.find(f'{commit.branch_point}@{repository.default_branch}', include_log=False)
    note = f"{revision or 'HEAD'} is on branch {commit.branch}; using the commit it is based on, {base}. The product does not have the branch's own commits."
    return commit.branch_point, commit.hash, str(commit), note


def archive_exists(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, method='HEAD'), timeout=60):
            return True
    except urllib.error.HTTPError as error:
        # S3 answers 403 instead of 404 for a missing key when listing the bucket is not allowed, as it is for the archives.
        if error.code in (403, 404):
            return False
        raise


def parse_args(argv):
    examples = ('Examples:\n'
                '  %(prog)s --list\n'
                '  %(prog)s --config wpe-x86_64-release --list\n'
                '  %(prog)s --config wpe-x86_64-release       # the checked-out commit\n'
                '  %(prog)s --config gtk-x86_64-release 325980@main\n'
                '  %(prog)s --config gtk-x86_64-release last  # the newest product\n')
    parser = argparse.ArgumentParser(description='Download and extract a built product uploaded by build.webkit.org.', epilog=examples, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--config', metavar='NAME', help='Which products, named as their S3 directory, e.g. "wpe-x86_64-release". Run with --list alone to see them all.')
    parser.add_argument('--list', action='store_true', help='List the configurations, or with --config, its newest products.')
    parser.add_argument('--limit', type=int, default=DEFAULT_LIST_LIMIT, help='How many products --list shows (default: %(default)s).')
    parser.add_argument('revision', nargs='?', help=f'The product to download: an identifier (N, N@main, or N.M@branch for release branches), a git hash or ref, or "{LAST}" for the newest product. Defaults to the checked-out commit.')
    args = parser.parse_args(argv)
    if not args.list and not args.config:
        parser.error('--config is required, except with --list alone')
    if args.list and args.revision:
        parser.error('--list does not take a revision')
    if args.limit < 0:
        parser.error('--limit cannot be negative')
    return args


def checked_out_revision():
    """(identifier, git_hash, name) of the checked-out commit, as resolve_revision() returns them, or None."""
    try:
        identifier, git_hash, name, _ = resolve_revision(None)
        return identifier, git_hash, name
    except Exception:
        return None


def warn_if_not_checked_out(product, checked_out, err=None):
    err = err or sys.stderr
    if not checked_out or (product.identifier is not None and product.identifier == checked_out[0]) or same_commit(product.git_hash, checked_out[1]):
        return
    matching = (product.git_hash or product.archive_revision)[:12]
    print(f'WARNING: the product is {product.archive_revision}, but the checked-out commit is {checked_out[2]}, so the tests in LayoutTests/ may not match it. To test with the matching tests: git checkout {matching}', file=err)


def main(argv=None, fetch=fetch_json, run=subprocess.call, out=sys.stdout, checked_out=checked_out_revision):
    args = parse_args(argv)
    configurations = load_configurations(read_config())

    if args.list and not args.config:
        for name in sorted(configurations):
            print(f'{name:<45} {configurations[name].builder}', file=out)
        return 0

    configuration = configurations.get(args.config)
    if not configuration:
        config_path = os.path.relpath(CONFIG_TEMPLATE_PATH, TOP_LEVEL_DIRECTORY)
        known = '\n  '.join(sorted(configurations))
        print(f'Unknown configuration "{args.config}". Known ones (from {config_path}):\n  {known}', file=sys.stderr)
        return 1

    if args.list:
        print(f'Newest products of {configuration.name} ({configuration.builder_url()}):', file=out)
        # islice() stops without reading one more product, which could need another request to build.webkit.org.
        for product in islice(products(configuration, fetch=fetch), args.limit):
            print('  ' + product.describe(), file=out)
        return 0

    if args.revision == LAST:
        product = next(products(configuration, fetch=fetch), None)
        if not product:
            print(f'{configuration.name} has no products on {configuration.builder_url()}', file=sys.stderr)
            return 1
        print(f'The newest product of {configuration.name} is {product.archive_revision}.', file=out, flush=True)
    else:
        try:
            identifier, git_hash, name, note = resolve_revision(args.revision)
        except ValueError as error:
            print(str(error), file=sys.stderr)
            return 1
        if not args.revision:
            print(f'No revision given: looking for the product of the checked-out commit, {name}.', file=out, flush=True)

        product, older, newer = find_product(products(configuration, fetch=fetch), identifier, git_hash, name)
        # The note says that the products of main are used for a commit on another branch. That is only true when the product found, or the
        # nearest ones suggested, are of main, and not when the configuration builds that branch (as with the release branches).
        used_main = product.archive_revision != name if product else bool(older or newer)
        if note and used_main:
            print(note, file=out, flush=True)
        if not product:
            wanted = name if args.revision else f'the checked-out commit, {name}'
            # The search is bounded (see MAX_BUILDS_TO_SEARCH and PRODUCTS_TO_SEARCH_PAST_AN_OLDER_ONE), so not finding a product doesn't prove there is none.
            print(f'{configuration.name} has no product for {wanted} among the builds searched (up to its last {MAX_BUILDS_TO_SEARCH}). Each build covers several commits, so only some of them have one.', file=sys.stderr)
            for label, nearest in (('Nearest older', older), ('Nearest newer', newer)):
                if nearest:
                    short_hash = (nearest.git_hash or '')[:12]
                    print(f'  {label}: {nearest.describe()}   (git checkout {short_hash})', file=sys.stderr)
            if older or newer:
                print(f'Pass one of them as the revision (checking it out first, so LayoutTests/ match), or "{LAST}" for the newest product.', file=sys.stderr)
            else:
                print(f'Run with --list to see its products, or pass "{LAST}" for the newest one.', file=sys.stderr)
            return 1

    url = configuration.archive_url(product.archive_revision)
    if not archive_exists(url):
        print(f'The archive of {product.archive_revision} is not on the server (any more): {url}', file=sys.stderr)
        return 1

    print(f'Downloading {product.archive_revision} of {configuration.name}: {url}', file=out, flush=True)
    status = run(configuration.download_command(product.archive_revision), cwd=TOP_LEVEL_DIRECTORY)
    if not status:
        status = run(configuration.extract_command(), cwd=TOP_LEVEL_DIRECTORY)
    if not status:
        # After the tools' own output, so that it is not lost in it.
        warn_if_not_checked_out(product, checked_out())
    return status
