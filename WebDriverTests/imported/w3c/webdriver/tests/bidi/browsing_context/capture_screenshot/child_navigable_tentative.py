import pytest

from tests.support.screenshot import DEFAULT_CONTENT, INNER_IFRAME_STYLE
from webdriver.bidi.modules.browsing_context import BoxOptions

pytestmark = pytest.mark.asyncio

# Capturing a child navigable is optional: https://github.com/w3c/webdriver-bidi/issues/441

# The top-level document has a 200px frame at (30, 30). That frame's document has a 150px frame at
# (20, 20), whose document has a green 100px box at its origin.
TOP_LEVEL_IFRAME_STYLE = """
    <style>
      body {
        margin: 0;
      }
      iframe {
        display: block;
        margin: 30px;
        border: 0;
        width: 200px;
        height: 200px;
      }
    </style>
"""

# The white background keeps the comparisons from depending on what a browser paints behind a child
# navigable's document.
MIDDLE_IFRAME_STYLE = """
    <style>
      html {
        background: white;
      }
      body {
        margin: 0;
      }
      iframe {
        display: block;
        margin: 20px;
        border: 0;
        width: 150px;
        height: 150px;
      }
    </style>
"""

child_domains = pytest.mark.parametrize(
    "child_domain", ["", "alt"], ids=["same_origin_child", "cross_origin_child"])
grandchild_domains = pytest.mark.parametrize(
    "grandchild_domain", ["", "alt"], ids=["same_origin_grandchild", "cross_origin_grandchild"])


def middle_document(iframe, grandchild_domain=""):
    return f"{MIDDLE_IFRAME_STYLE}{iframe(f'{INNER_IFRAME_STYLE}{DEFAULT_CONTENT}', domain=grandchild_domain)}"


async def navigate(bidi_session, context, url):
    await bidi_session.browsing_context.navigate(
        context=context["context"], url=url, wait="complete")


async def get_child_context(bidi_session, top_context):
    contexts = await bidi_session.browsing_context.get_tree(root=top_context["context"])
    return contexts[0]["children"][0]


@grandchild_domains
@child_domains
async def test_child(bidi_session, top_context, inline, iframe, compare_png_bidi,
                     child_domain, grandchild_domain):
    # The child's document as a top-level document, clipped to the child's viewport.
    await navigate(bidi_session, top_context, inline(middle_document(iframe)))
    reference_data = await bidi_session.browsing_context.capture_screenshot(
        context=top_context["context"], clip=BoxOptions(x=0, y=0, width=200, height=200))

    await navigate(bidi_session, top_context,
                   inline(f"{TOP_LEVEL_IFRAME_STYLE}{iframe(middle_document(iframe, grandchild_domain), domain=child_domain)}"))
    child_context = await get_child_context(bidi_session, top_context)
    data = await bidi_session.browsing_context.capture_screenshot(
        context=child_context["context"])

    comparison = await compare_png_bidi(data, reference_data)
    assert comparison.equal()


@grandchild_domains
@child_domains
async def test_child_document_origin(bidi_session, top_context, inline, iframe, compare_png_bidi,
                                     child_domain, grandchild_domain):
    # The parent shows only the top left 100px of the child, but the child's capture is of
    # its own document. That document is 170px wide (the frame's right margin doesn't count),
    # so the clip stays inside it.
    clip = BoxOptions(x=100, y=100, width=60, height=60)

    await navigate(bidi_session, top_context, inline(middle_document(iframe)))
    reference_data = await bidi_session.browsing_context.capture_screenshot(
        context=top_context["context"], clip=clip)

    small_top_level_iframe_style = TOP_LEVEL_IFRAME_STYLE.replace("200px", "100px")
    await navigate(bidi_session, top_context,
                   inline(f"{small_top_level_iframe_style}{iframe(middle_document(iframe, grandchild_domain), domain=child_domain)}"))
    child_context = await get_child_context(bidi_session, top_context)
    data = await bidi_session.browsing_context.capture_screenshot(
        context=child_context["context"], origin="document", clip=clip)

    comparison = await compare_png_bidi(data, reference_data)
    assert comparison.equal()


def quadrants(size):
    # Four squares of different colors.
    return f"""
    <div style='display: grid; grid-template: {size}px {size}px / {size}px {size}px'>
      <div style='background: red'></div>
      <div style='background: lime'></div>
      <div style='background: blue'></div>
      <div style='background: yellow'></div>
    </div>
"""


QUADRANTS = quadrants(100)
NO_MARGIN_STYLE = "<style>body { margin: 0; }</style>"


@child_domains
async def test_child_in_zoomed_element(bidi_session, top_context, inline, iframe, compare_png_bidi,
                                       child_domain):
    # CSS zoom on the parent scales the child's device pixel ratio, and the capture is of the
    # child's whole viewport at that ratio, so it matches the same content zoomed at the top level.
    await navigate(bidi_session, top_context,
                   inline(f"{NO_MARGIN_STYLE}<div style='zoom: 2'>{QUADRANTS}</div>"))
    reference_data = await bidi_session.browsing_context.capture_screenshot(
        context=top_context["context"], clip=BoxOptions(x=0, y=0, width=400, height=400))

    await navigate(bidi_session, top_context,
                   inline(f"{TOP_LEVEL_IFRAME_STYLE}<div style='zoom: 2'>{iframe(f'{NO_MARGIN_STYLE}{QUADRANTS}', domain=child_domain)}</div>"))
    child_context = await get_child_context(bidi_session, top_context)
    data = await bidi_session.browsing_context.capture_screenshot(context=child_context["context"])

    comparison = await compare_png_bidi(data, reference_data)
    assert comparison.equal()


@child_domains
@pytest.mark.parametrize("clip", [None, BoxOptions(x=150, y=0, width=150, height=150)],
                         ids=["no_clip", "clip"])
async def test_child_in_zoomed_element_document_origin(bidi_session, top_context, inline, iframe,
                                                       compare_png_bidi, child_domain, clip):
    # The child's document is 300px, bigger than its viewport, and both its size and the clip are
    # in the child's CSS pixels, which CSS zoom on the parent scales. The reference uses the document
    # origin too, so that it doesn't depend on the size of the window.
    await navigate(bidi_session, top_context,
                   inline(f"{NO_MARGIN_STYLE}<div style='zoom: 2'>{quadrants(150)}</div>"))
    reference_clip = BoxOptions(x=0, y=0, width=600, height=600) if clip is None else \
        BoxOptions(x=clip["x"] * 2, y=clip["y"] * 2, width=clip["width"] * 2, height=clip["height"] * 2)
    reference_data = await bidi_session.browsing_context.capture_screenshot(
        context=top_context["context"], origin="document", clip=reference_clip)

    await navigate(bidi_session, top_context,
                   inline(f"{TOP_LEVEL_IFRAME_STYLE}<div style='zoom: 2'>{iframe(f'{NO_MARGIN_STYLE}{quadrants(150)}', domain=child_domain)}</div>"))
    child_context = await get_child_context(bidi_session, top_context)
    data = await bidi_session.browsing_context.capture_screenshot(
        context=child_context["context"], origin="document", clip=clip)

    comparison = await compare_png_bidi(data, reference_data)
    assert comparison.equal()
