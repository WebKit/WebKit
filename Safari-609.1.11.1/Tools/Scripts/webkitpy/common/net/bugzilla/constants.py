# Example: 2010-01-20 14:31 PST
# FIXME: Some bugzilla dates seem to have seconds in them?
# Python does not support timezones out of the box.
# Assume that bugzilla always uses PST (which is true for bugs.webkit.org)
BUGZILLA_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"
