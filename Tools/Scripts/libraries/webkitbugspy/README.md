# webkitbugspy

Provides a shared API for various bug trackers.

## Requirements

- webkitcorepy

## Usage

The `webkitbugspy` library implements a generic issue tracker API compatible with multiple bug and issue trackers. To interact with an `Issue`, first instantiate a tracker:

```
from webkitbugspy import github

tracker = github.Tracker('https://github.com/WebKit/WebKit')
issue = tracker.issue(1)
```

You should register all trackers your project interacts with so that an `Issue` in one tracker can cross-reference issues in other trackers:

```
from webkitbugspy import bugzilla, github

Tracker.register(bugzilla.Tracker('https://bugs.webkit.org', res=[
    re.compile(r'\Ahttps?://webkit.org/b/(?P<id>\d+)\Z'),
    re.compile(r'\Awebkit.org/b/(?P<id>\d+)\Z'),
]))
Tracker.register(github.Tracker('https://github.com/WebKit/WebKit'))

print(Tracker.from_string('https://github.com/WebKit/WebKit/issues/47').references)
```
