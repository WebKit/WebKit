#!/usr/bin/env python

import argparse
import json
import logging
import os
import re
import subprocess
import sys
import urlparse

from StringIO import StringIO
from abc import ABCMeta, abstractmethod, abstractproperty
from collections import defaultdict
from fnmatch import fnmatch

import _env
import html5lib

def get_git_func(repo_path):
    def git(cmd, *args):
        full_cmd = ["git", cmd] + list(args)
        return subprocess.check_output(full_cmd, cwd=repo_path, stderr=subprocess.STDOUT)
    return git


def is_git_repo(tests_root):
    return os.path.exists(os.path.join(tests_root, ".git"))


_repo_root = None
def get_repo_root():
    global _repo_root
    if _repo_root is None:
        git = get_git_func(os.path.dirname(__file__))
        _repo_root = git("rev-parse", "--show-toplevel").rstrip()
    return _repo_root


manifest_name = "MANIFEST.json"
ref_suffixes = ["_ref", "-ref"]
wd_pattern = "*.py"
blacklist = ["/", "/tools/", "/resources/", "/common/", "/conformance-checkers/", "_certs"]


logging.basicConfig()
logger = logging.getLogger("manifest")
logger.setLevel(logging.DEBUG)


def rel_path_to_url(rel_path, url_base="/"):
    assert not os.path.isabs(rel_path)
    if url_base[0] != "/":
        url_base = "/" + url_base
    if url_base[-1] != "/":
        url_base += "/"
    return url_base + rel_path.replace(os.sep, "/")


def url_to_rel_path(url, url_base):
    url_path = urlparse.urlsplit(url).path
    if not url_path.startswith(url_base):
        raise ValueError, url
    url_path = url_path[len(url_base):]
    return url_path


def is_blacklisted(url):
    for item in blacklist:
        if item == "/":
            if "/" not in url[1:]:
                return True
        elif url.startswith(item):
            return True
    return False


class ManifestItem(object):
    __metaclass__ = ABCMeta

    item_type = None

    def __init__(self):
        self.manifest = None

    @abstractmethod
    def key(self):
        pass

    def __eq__(self, other):
        if not hasattr(other, "key"):
            return False
        return self.key() == other.key()

    def __hash__(self):
        return hash(self.key())

    @abstractmethod
    def to_json(self):
        raise NotImplementedError

    @classmethod
    def from_json(self, manifest, obj):
        raise NotImplementedError

    @abstractproperty
    def id(self):
        pass


class URLManifestItem(ManifestItem):
    def __init__(self, url, url_base="/"):
        ManifestItem.__init__(self)
        self.url = url
        self.url_base = url_base

    @property
    def id(self):
        return self.url

    def key(self):
        return self.item_type, self.url

    @property
    def path(self):
        return url_to_rel_path(self.url, self.url_base)

    def to_json(self):
        rv = {"url": self.url}
        return rv

    @classmethod
    def from_json(cls, manifest, obj):
        return cls(obj["url"],
                   url_base=manifest.url_base)


class TestharnessTest(URLManifestItem):
    item_type = "testharness"

    def __init__(self, url, url_base="/", timeout=None):
        URLManifestItem.__init__(self, url, url_base=url_base)
        self.timeout = timeout

    def to_json(self):
        rv = {"url": self.url}
        if self.timeout is not None:
            rv["timeout"] = self.timeout
        return rv

    @classmethod
    def from_json(cls, manifest, obj):
        return cls(obj["url"],
                   url_base=manifest.url_base,
                   timeout=obj.get("timeout"))


class RefTest(URLManifestItem):
    item_type = "reftest"

    def __init__(self, url, ref_url, ref_type, url_base="/", timeout=None):
        URLManifestItem.__init__(self, url, url_base=url_base)
        if ref_type not in ["==", "!="]:
            raise ValueError, "Unrecognised ref_type %s" % ref_type
        self.ref_url = ref_url
        self.ref_type = ref_type
        self.timeout = timeout

    @property
    def id(self):
        return (self.url, self.ref_type, self.ref_url)

    def key(self):
        return self.item_type, self.url, self.ref_type, self.ref_url

    def to_json(self):
        rv = {"url": self.url,
              "ref_type": self.ref_type,
              "ref_url": self.ref_url}
        if self.timeout is not None:
            rv["timeout"] = self.timeout
        return rv

    @classmethod
    def from_json(cls, manifest, obj):
        return cls(obj["url"],
                   obj["ref_url"],
                   obj["ref_type"],
                   url_base=manifest.url_base,
                   timeout=obj.get("timeout"))


class ManualTest(URLManifestItem):
    item_type = "manual"

class Stub(URLManifestItem):
    item_type = "stub"

class Helper(URLManifestItem):
    item_type = "helper"

class WebdriverSpecTest(ManifestItem):
    item_type = "wdspec"

    def __init__(self, path):
        ManifestItem.__init__(self)
        self.path = path

    @property
    def id(self):
        return self.path

    def to_json(self):
        return {"path": self.path}

    def key(self):
        return self.path

    @classmethod
    def from_json(cls, manifest, obj):
        return cls(path=obj["path"])


class ManifestError(Exception):
    pass

item_types = ["testharness", "reftest", "manual", "helper", "stub", "wdspec"]

class Manifest(object):
    def __init__(self, git_rev=None, url_base="/"):
        # Dict of item_type: {path: set(manifest_items)}
        self._data = dict((item_type, defaultdict(set))
                          for item_type in item_types)
        self.rev = git_rev
        self.url_base = url_base
        self.local_changes = LocalChanges(self)

    def _included_items(self, include_types=None):
        if include_types is None:
            include_types = item_types

        for item_type in include_types:
            paths = self._data[item_type].copy()
            for local_types, local_paths in self.local_changes.itertypes(item_type):
                for path, items in local_paths.iteritems():
                    paths[path] = items
                for path in self.local_changes.iterdeleted():
                    if path in paths:
                        del paths[path]

            yield item_type, paths

    def contains_path(self, path):
        return any(path in paths for _, paths in self._included_items())

    def add(self, item):
        self._data[item.item_type][item.path].add(item)
        item.manifest = self

    def extend(self, items):
        for item in items:
            self.add(item)

    def remove_path(self, path):
        for item_type in item_types:
            if path in self._data[item_type]:
                del self._data[item_type][path]

    def itertypes(self, *types):
        if not types:
            types = None
        for item_type, items in self._included_items(types):
            for item in sorted(items.items()):
                yield item

    def __iter__(self):
        for item in self.itertypes():
            yield item

    def __getitem__(self, path):
        for _, paths in self._included_items():
            if path in paths:
                return paths[path]
        raise KeyError

    def _committed_with_path(self, rel_path):
        rv = set()
        for paths_items in self._data.itervalues():
            rv |= paths_items.get(rel_path, set())
        return rv

    def _committed_paths(self):
        rv = set()
        for paths_items in self._data.itervalues():
            rv |= set(paths_items.keys())
        return rv

    def update(self,
               tests_root,
               url_base,
               new_rev,
               committed_changes=None,
               local_changes=None,
               remove_missing_local=False):

        if local_changes is None:
            local_changes = {}

        if committed_changes is not None:
            for rel_path, status in committed_changes:
                self.remove_path(rel_path)
                if status == "modified":
                    use_committed = rel_path in local_changes
                    manifest_items = get_manifest_items(tests_root,
                                                        rel_path,
                                                        url_base,
                                                        use_committed=use_committed)
                    self.extend(manifest_items)

        self.local_changes = LocalChanges(self)

        local_paths = set()
        for rel_path, status in local_changes.iteritems():
            local_paths.add(rel_path)

            if status == "modified":
                existing_items = self._committed_with_path(rel_path)
                local_items = set(get_manifest_items(tests_root,
                                                     rel_path,
                                                     url_base,
                                                     use_committed=False))

                updated_items = local_items - existing_items
                self.local_changes.extend(updated_items)
            else:
                self.local_changes.add_deleted(path)

        if remove_missing_local:
            for path in self._committed_paths() - local_paths:
                self.local_changes.add_deleted(path)

        if new_rev is not None:
            self.rev = new_rev
        self.url_base = url_base

    def to_json(self):
        out_items = {
            item_type: sorted(
                test.to_json()
                for _, tests in items.iteritems()
                for test in tests
            )
            for item_type, items in self._data.iteritems()
        }

        rv = {"url_base": self.url_base,
              "rev": self.rev,
              "local_changes": self.local_changes.to_json(),
              "items": out_items}
        return rv

    @classmethod
    def from_json(cls, obj):
        self = cls(git_rev=obj["rev"],
                   url_base=obj.get("url_base", "/"))
        if not hasattr(obj, "iteritems"):
            raise ManifestError

        item_classes = {"testharness": TestharnessTest,
                        "reftest": RefTest,
                        "manual": ManualTest,
                        "helper": Helper,
                        "stub": Stub,
                        "wdspec": WebdriverSpecTest}

        for k, values in obj["items"].iteritems():
            if k not in item_types:
                raise ManifestError
            for v in values:
                manifest_item = item_classes[k].from_json(self, v)
                self.add(manifest_item)
        self.local_changes = LocalChanges.from_json(self, obj["local_changes"])
        return self

class LocalChanges(object):
    def __init__(self, manifest):
        self.manifest = manifest
        self._data = dict((item_type, defaultdict(set)) for item_type in item_types)
        self._deleted = set()

    def add(self, item):
        self._data[item.item_type][item.path].add(item)
        item.manifest = self.manifest

    def extend(self, items):
        for item in items:
            self.add(item)

    def add_deleted(self, path):
        self._deleted.add(path)

    def is_deleted(self, path):
        return path in self._deleted

    def itertypes(self, *types):
        for item_type in types:
            yield item_type, self._data[item_type]

    def iterdeleted(self):
        for item in self._deleted:
            yield item

    def __getitem__(self, item_type):
        return self._data[item_type]

    def to_json(self):
        rv = {"items": defaultdict(dict),
              "deleted": []}

        rv["deleted"].extend(self._deleted)

        for test_type, paths in self._data.iteritems():
            for path, tests in paths.iteritems():
                rv["items"][test_type][path] = [test.to_json() for test in tests]

        return rv

    @classmethod
    def from_json(cls, url_base, obj):
        self = cls(url_base)
        if not hasattr(obj, "iteritems"):
            raise ManifestError

        item_classes = {"testharness": TestharnessTest,
                        "reftest": RefTest,
                        "manual": ManualTest,
                        "helper": Helper,
                        "stub": Stub,
                        "wdspec": WebdriverSpecTest}

        for test_type, paths in obj["items"].iteritems():
            for path, tests in paths.iteritems():
                for test in tests:
                    manifest_item = item_classes[test_type].from_json(self.manifest, test)
                    self.add(manifest_item)

        for item in obj["deleted"]:
            self.add_deleted(item)

        return self


def get_ref(path):
    base_path, filename = os.path.split(path)
    name, ext = os.path.splitext(filename)
    for suffix in ref_suffixes:
        possible_ref = os.path.join(base_path, name + suffix + ext)
        if os.path.exists(possible_ref):
            return possible_ref


def markup_type(ext):
    if not ext:
        return None
    if ext[0] == ".":
        ext = ext[1:]
    if ext in ["html", "htm"]:
        return "html"
    elif ext in ["xhtml", "xht"]:
        return "xhtml"
    elif ext == "svg":
        return "svg"
    return None


def get_timeout_meta(root):
    return root.findall(".//{http://www.w3.org/1999/xhtml}meta[@name='timeout']")


def get_testharness_scripts(root):
    return root.findall(".//{http://www.w3.org/1999/xhtml}script[@src='/resources/testharness.js']")


def get_reference_links(root):
    match_links = root.findall(".//{http://www.w3.org/1999/xhtml}link[@rel='match']")
    mismatch_links = root.findall(".//{http://www.w3.org/1999/xhtml}link[@rel='mismatch']")
    return match_links, mismatch_links


def get_file(base_path, rel_path, use_committed):
    if use_committed:
        git = get_git_func(os.path.dirname(__file__))
        blob = git("show", "HEAD:%s" % rel_path)
        file_obj = ContextManagerStringIO(blob)
    else:
        path = os.path.join(base_path, rel_path)
        file_obj = open(path)
    return file_obj

class ContextManagerStringIO(StringIO):
    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        self.close()

def get_manifest_items(tests_root, rel_path, url_base, use_committed=False):
    if os.path.isdir(rel_path):
        return []

    url = rel_path_to_url(rel_path, url_base)
    path = os.path.join(tests_root, rel_path)

    if not use_committed and not os.path.exists(path):
        return []

    dir_path, filename = os.path.split(path)
    name, ext = os.path.splitext(filename)
    rel_dir_tree = rel_path.split(os.path.sep)

    file_markup_type = markup_type(ext)

    if filename.startswith("MANIFEST") or filename.startswith("."):
        return []

    if is_blacklisted(url):
        return []

    if name.startswith("stub-"):
        return [Stub(url)]

    if name.lower().endswith("-manual"):
        return [ManualTest(url)]

    if filename.endswith(".worker.js"):
        return [TestharnessTest(url[:-3])]

    ref_list = []

    for suffix in ref_suffixes:
        if name.endswith(suffix):
            return [Helper(url)]
        elif os.path.exists(os.path.join(dir_path, name + suffix + ext)):
            ref_url, ref_ext = url.rsplit(".", 1)
            ref_url = ref_url + suffix + ext
            #Need to check if this is the right reftype
            ref_list = [RefTest(url, ref_url, "==")]

    # wdspec tests are in subdirectories of /webdriver excluding __init__.py
    # files.
    if (rel_dir_tree[0] == "webdriver" and
        len(rel_dir_tree) > 2 and
        filename != "__init__.py" and
        fnmatch(filename, wd_pattern)):
        return [WebdriverSpecTest(rel_path)]

    if file_markup_type:
        timeout = None

        parser = {"html":lambda x:html5lib.parse(x, treebuilder="etree"),
                  "xhtml":ElementTree.parse,
                  "svg":ElementTree.parse}[file_markup_type]

        with get_file(tests_root, rel_path, use_committed) as f:
            try:
                tree = parser(f)
            except:
                return [Helper(url)]

        if hasattr(tree, "getroot"):
            root = tree.getroot()
        else:
            root = tree

        timeout_nodes = get_timeout_meta(root)
        if timeout_nodes:
            timeout_str = timeout_nodes[0].attrib.get("content", None)
            if timeout_str and timeout_str.lower() == "long":
                try:
                    timeout = timeout_str.lower()
                except:
                    pass

        if get_testharness_scripts(root):
            return [TestharnessTest(url, timeout=timeout)]
        else:
            match_links, mismatch_links = get_reference_links(root)
            for item in match_links + mismatch_links:
                ref_url = urlparse.urljoin(url, item.attrib["href"])
                ref_type = "==" if item.attrib["rel"] == "match" else "!="
                reftest = RefTest(url, ref_url, ref_type, timeout=timeout)
                if reftest not in ref_list:
                    ref_list.append(reftest)
            return ref_list

    return [Helper(url)]


def abs_path(path):
    return os.path.abspath(path)


def chunks(data, n):
    for i in range(0, len(data) - 1, n):
        yield data[i:i+n]


class TestTree(object):
    def __init__(self, tests_root, url_base):
        self.tests_root = tests_root
        self.url_base = url_base

    def current_rev(self):
        pass

    def local_changes(self):
        pass

    def comitted_changes(self, base_rev=None):
        pass


class GitTree(TestTree):
    def __init__(self, tests_root, url_base):
        TestTree.__init__(self, tests_root, url_base)
        self.git = self.setup_git()

    def setup_git(self):
        assert is_git_repo(self.tests_root)
        return get_git_func(self.tests_root)

    def current_rev(self):
        return self.git("rev-parse", "HEAD").strip()

    def local_changes(self, path=None):
        # -z is stable like --porcelain; see the git status documentation for details
        cmd = ["status", "-z", "--ignore-submodules=all"]
        if path is not None:
            cmd.extend(["--", path])

        rv = {}

        data = self.git(*cmd)
        if data == "":
            return rv

        assert data[-1] == "\0"
        f = StringIO(data)

        while f.tell() < len(data):
            # First two bytes are the status in the stage (index) and working tree, respectively
            staged = f.read(1)
            worktree = f.read(1)
            assert f.read(1) == " "

            if staged == "R":
                # When a file is renamed, there are two files, the source and the destination
                files = 2
            else:
                files = 1

            filenames = []

            for i in range(files):
                filenames.append("")
                char = f.read(1)
                while char != "\0":
                    filenames[-1] += char
                    char = f.read(1)

            if not is_blacklisted(rel_path_to_url(filenames[0], self.url_base)):
                rv.update(self.local_status(staged, worktree, filenames))

        return rv

    def committed_changes(self, base_rev=None):
        if base_rev is None:
            logger.debug("Adding all changesets to the manifest")
            return [(item, "modified") for item in self.paths()]

        logger.debug("Updating the manifest from %s to %s" % (base_rev, self.current_rev()))
        rv = []
        data  = self.git("diff", "-z", "--name-status", base_rev + "..HEAD")
        items = data.split("\0")
        for status, filename in chunks(items, 2):
            if is_blacklisted(rel_path_to_url(filename, self.url_base)):
                continue
            if status == "D":
                rv.append((filename, "deleted"))
            else:
                rv.append((filename, "modified"))
        return rv

    def paths(self):
        data = self.git("ls-tree", "--name-only", "--full-tree", "-r", "HEAD")
        return [item for item in data.split("\n") if not item.endswith(os.path.sep)]

    def local_status(self, staged, worktree, filenames):
        # Convert the complex range of statuses that git can have to two values
        # we care about; "modified" and "deleted" and return a dictionary mapping
        # filenames to statuses

        rv = {}

        if (staged, worktree) in [("D", "D"), ("A", "U"), ("U", "D"), ("U", "A"),
                                  ("D", "U"), ("A", "A"), ("U", "U")]:
            raise Exception("Can't operate on tree containing unmerged paths")

        if staged == "R":
            assert len(filenames) == 2
            dest, src = filenames
            rv[dest] = "modified"
            rv[src] = "deleted"
        else:
            assert len(filenames) == 1

            filename = filenames[0]

            if staged == "D" or worktree == "D":
                # Actually if something is deleted in the index but present in the worktree
                # it will get included by having a status of both "D " and "??".
                # It isn't clear whether that's a bug
                rv[filename] = "deleted"
            elif staged == "?" and worktree == "?":
                # A new file. If it's a directory, recurse into it
                if os.path.isdir(os.path.join(self.tests_root, filename)):
                    rv.update(self.local_changes(filename))
                else:
                    rv[filename] = "modified"
            else:
                rv[filename] = "modified"

        return rv

class NoVCSTree(TestTree):
    """Subclass that doesn't depend on git"""

    ignore = ["*.py[c|0]", "*~", "#*"]

    def current_rev(self):
        return None

    def local_changes(self):
        # Put all files into local_changes and rely on Manifest.update to de-dupe
        # changes that in fact comitted at the base rev.

        rv = []
        for dir_path, dir_names, filenames in os.walk(self.tests_root):
            for filename in filenames:
                if any(fnmatch(filename, pattern) for pattern in self.ignore):
                    continue
                rel_path = os.path.relpath(os.path.join(dir_path, filename),
                                           self.tests_root)
                if is_blacklisted(rel_path_to_url(rel_path, self.url_base)):
                    continue
                rv.append((rel_path, "modified"))
        return dict(rv)

    def committed_changes(self, base_rev=None):
        return None


def load(manifest_path):
    if os.path.exists(manifest_path):
        logger.debug("Opening manifest at %s" % manifest_path)
    else:
        logger.debug("Creating new manifest at %s" % manifest_path)
    try:
        with open(manifest_path) as f:
            manifest = Manifest.from_json(json.load(f))
    except IOError as e:
        manifest = Manifest(None)

    return manifest


def update(tests_root, url_base, manifest, ignore_local=False):
    global ElementTree
    global html5lib

    try:
        from xml.etree import cElementTree as ElementTree
    except ImportError:
        from xml.etree import ElementTree

    import html5lib

    if is_git_repo(tests_root):
        tests_tree = GitTree(tests_root, url_base)
        remove_missing_local = False
    else:
        tests_tree = NoVCSTree(tests_root, url_base)
        remove_missing_local = not ignore_local

    if not ignore_local:
        local_changes = tests_tree.local_changes()
    else:
        local_changes = None

    manifest.update(tests_root,
                    url_base,
                    tests_tree.current_rev(),
                    tests_tree.committed_changes(manifest.rev),
                    local_changes,
                    remove_missing_local=remove_missing_local)


def write(manifest, manifest_path):
    with open(manifest_path, "w") as f:
        json.dump(manifest.to_json(), f, sort_keys=True, indent=2, separators=(',', ': '))


def update_from_cli(**kwargs):
    tests_root = kwargs["tests_root"]
    path = kwargs["path"]
    assert tests_root is not None
    if not kwargs.get("rebuild", False):
        manifest = load(path)
    else:
        manifest = Manifest(None)

    logger.info("Updating manifest")
    update(tests_root,
           kwargs["url_base"],
           manifest,
           ignore_local=kwargs.get("ignore_local", False))
    write(manifest, path)


def abs_path(path):
    return os.path.abspath(os.path.expanduser(path))


def create_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-p", "--path", type=abs_path, help="Path to manifest file.")
    parser.add_argument(
        "--tests-root", type=abs_path, help="Path to root of tests.")
    parser.add_argument(
        "-r", "--rebuild", action="store_true", default=False,
        help="Force a full rebuild of the manifest.")
    parser.add_argument(
        "--ignore-local", action="store_true", default=False,
        help="Don't include uncommitted local changes in the manifest.")
    parser.add_argument(
        "--url-base", action="store", default="/",
        help="Base url to use as the mount point for tests in this manifest.")
    return parser

if __name__ == "__main__":
    opts = create_parser().parse_args()

    if opts.tests_root is None:
        opts.tests_root = get_repo_root()

    if opts.path is None:
        opts.path = os.path.join(opts.tests_root, "MANIFEST.json")

    update_from_cli(**vars(opts))
