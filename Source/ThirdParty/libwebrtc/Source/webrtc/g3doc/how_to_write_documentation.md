# How to write WebRTC documentation

<?% config.freshness.owner = 'titovartem' %?>
<?% config.freshness.reviewed = '2021-03-01' %?>

## Audience

Engineers and tech writers who wants to contribute to WebRTC documentation

## Conceptual documentation

Conceptual documentation provides overview of APIs or systems. Examples can
be threading model of a particular module or data life cycle. Conceptual
documentation can skip some edge cases in favor of clarity. The main point
is to impart understanding.

Conceptual documentation often cannot be embedded directly within the source
code because it usually describes multiple APIs and entites, so the only
logical place to document such complex behavior is through a separate
conceptual document.

A concept document needs to be useful to both experts and novices. Moreover,
it needs to emphasize clarity, so it often needs to sacrifice completeness
and sometimes strict accuracy. That's not to say a conceptual document should
intentionally be inaccurate. It just means that is should focus more on common
usage and leave rare ones or side effects for class/function level comments.

In the WebRTC repo, conceptual documentation is located in `g3doc` subfolders
of related components. To add a new document for the component `Foo` find a
`g3doc` subfolder for this component and create a `.md` file there with
desired documentation. If there is no `g3doc` subfolder, create a new one;

When you want to specify a link from one page to another - use the absolute
path:

```
[My document](/module/g3doc/my_document.md)
```

If you are a Googler also please specify an owner, who will be responsible for
keeping this documentation updated, by adding the next lines at the beginning
of your `.md` file immediately after page title:

```markdown
<?\% config.freshness.owner = '<user name>' %?>
<?\% config.freshness.reviewed = '<last review date in format yyyy-mm-dd>' %?>
```

If you want to configure the owner for all pages under a directory, create a
`g3doc.lua` file in that directory with the content:

```lua
config = super()
config.freshness.owner = '<user name>'
return config
```

After the document is ready you should add it into `/g3doc/sitemap.md`, so it
will be discoverable by others.

### Documentation format

The documentation is written in GitHub Markdown
([spec](https://github.github.com/gfm/#:~:text=GitHub%20Flavored%20Markdown%2C%20often%20shortened,a%20strict%20superset%20of%20CommonMark.)).

## Class/function level comments

Documentation of specific classes and function APIs and their usage, including
their purpose, is embedded in the .h files defining that API. See
[C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
for pointers on how to write API documentatin in .h files.
