.. _markdown:

Markdown
########

Markdown lets you write comments in a structured plain-text format that
Bugzilla displays as HTML. For example, you can make part of a comment bold
or italic, add headings and lists, or include a code block.

Bugzilla uses `GitHub Flavored Markdown <https://github.github.com/gfm/>`_.
Inline images and raw HTML are not supported; raw HTML is shown as text.
In addition to standard Markdown, these extensions are enabled:

- tables
- strikethrough
- automatic links for bare URLs

Fenced code blocks that name a language, such as ``swift``, are syntax
highlighted. Bugzilla's own shorthand, such as ``bug 12345`` and
``comment 3``, still becomes a link in normal text, but not inside code.

Turning Markdown on and off
===========================

Markdown is controlled by the ``Enable Markdown support when composing my
comments`` setting in your :ref:`user-preferences`, which is on by default.
When it is on, each comment you submit is stored as Markdown. Comments
written while it is off, and comments that were written before Markdown
support existed, are always shown as plain text.

To write a single comment as plain text while the setting is on, start the
comment with ``#[markdown(off)]`` on its own line. Bugzilla then shows the
comment as plain text, still linking URLs and shorthand like ``bug 12345``.

The :guilabel:`Preview` tab shows how a comment will be rendered.
