# webkitscmpy

Provides a utilities for interacting with a git or svn repository.

## Requirements

- webkitcorepy
- fasteners
- monotonic
- whichcraft
- xmltodict

## Command Line

The `git-webkit` command supports a common set of basic repository manipulations. Most notably:

`git-webkit find <ref>`: Print out commit information for a git ref, Subversion revision or identifier.

`git-webkit checkout <ref>`: Move the current local repository to the provided git ref, Subversion revision or identifier.

`git-webkit canonicalize`: Standardize commit authorship and put identifiers into the commit message.

## Usage

The `webkitscmpy` library provides a repository abstraction for both local and remote repositories. To instantiate a
repository object, use the `local.Scm.from_path` and `remote.Scm.from_url` functions.

```
from webkitscmpy import local, remote

on_disk = local.Scm.from_path(<path>)
subversion = remote.Scm.from_url('https://svn.webkit.org/repository/webkit')
github = remote.Scm.from_url('https://github.com/WebKit/WebKit')
```

While the abstraction layer is consistent for all implementations not all implementation support every feature. For
example, remote repositories do not have a `checkout` command available.

Each repository keeps a list of contributors, which can be primed and passed into the repository object:

```
from webkitscmpy import local, Contributor
contributors = Contributor.Mapping()
contributors.create('Jonathan Bedard', 'jbedard@apple.com')
local.Scm.from_path(<path>, contributors=contributors)
```
