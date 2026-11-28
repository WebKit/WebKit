import pytest
from webdriver.bidi.modules.script import ContextTarget

pytestmark = pytest.mark.asyncio

# Without a root, getTree lists the user agent's top-level traversable set,
# which is an ordered set that new top-level traversables are appended to.


async def test_cross_site_navigation(bidi_session, top_context, inline):
    url = inline("<div>foo</div>")
    url_alt = inline("<div>foo</div>", domain="alt")

    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=url, wait="complete"
    )

    expected_contexts = [top_context["context"]]
    for type_hint in ["tab", "window"]:
        new_context = await bidi_session.browsing_context.create(type_hint=type_hint)
        await bidi_session.browsing_context.navigate(
            context=new_context["context"], url=url, wait="complete"
        )
        expected_contexts.append(new_context["context"])

    # A browser may move a top-level traversable to a new process when it
    # navigates cross-site, so the oldest one can end up in the newest process.
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=url_alt, wait="complete"
    )

    contexts = await bidi_session.browsing_context.get_tree(max_depth=0)
    assert [info["context"] for info in contexts] == expected_contexts


async def test_window_open(bidi_session, top_context, inline):
    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=inline("<div>foo</div>"), wait="complete"
    )

    # Popups opened by the same page may share its process.
    expected_contexts = [top_context["context"]]
    for _ in range(3):
        result = await bidi_session.script.evaluate(
            expression=f"""window.open("{inline("<div>bar</div>")}", "_blank")""",
            target=ContextTarget(top_context["context"]),
            await_promise=False,
        )
        assert result["type"] == "window"
        expected_contexts.append(result["value"]["context"])

    contexts = await bidi_session.browsing_context.get_tree(max_depth=0)
    assert [info["context"] for info in contexts] == expected_contexts
