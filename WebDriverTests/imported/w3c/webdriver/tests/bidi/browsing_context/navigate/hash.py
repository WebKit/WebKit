import pytest

from webdriver.bidi.modules.script import ContextTarget

from .. import navigate_and_assert
from ... import any_string

pytestmark = pytest.mark.asyncio

PAGE_EMPTY = "/webdriver/tests/bidi/browsing_context/support/empty.html"
PAGE_EMPTY_WITH_HASH_FOO = f"{PAGE_EMPTY}#foo"
PAGE_OTHER = "/webdriver/tests/bidi/browsing_context/support/other.html"


@pytest.mark.parametrize(
    "hash_before, hash_after",
    [
        ("", "#foo"),
        ("#foo", "#bar"),
        ("#foo", "#foo"),
        ("#bar", ""),
    ],
    ids=[
        "without hash to with hash",
        "with different hashes",
        "with identical hashes",
        "with hash to without hash",
    ],
)
async def test_navigate_in_the_same_document(
    bidi_session, new_tab, url, hash_before, hash_after
):
    await navigate_and_assert(bidi_session, new_tab, url(PAGE_EMPTY + hash_before))
    await navigate_and_assert(bidi_session, new_tab, url(PAGE_EMPTY + hash_after))


@pytest.mark.parametrize(
    "url_before, url_after",
    [
        (PAGE_EMPTY_WITH_HASH_FOO, f"{PAGE_OTHER}#foo"),
        (PAGE_EMPTY_WITH_HASH_FOO, f"{PAGE_OTHER}#bar"),
    ],
    ids=[
        "with identical hashes",
        "with different hashes",
    ],
)
async def test_navigate_different_documents(
    bidi_session, new_tab, url, url_before, url_after
):
    await navigate_and_assert(bidi_session, new_tab, url(url_before))
    await navigate_and_assert(bidi_session, new_tab, url(url_after))


async def test_navigate_in_iframe(bidi_session, inline, new_tab):
    frame_start_url = inline("frame")
    url_before = inline(f"<iframe src='{frame_start_url}'></iframe>")
    contexts = await navigate_and_assert(bidi_session, new_tab, url_before)

    assert len(contexts[0]["children"]) == 1
    frame = contexts[0]["children"][0]
    assert frame["url"] == frame_start_url

    url_after = f"{frame_start_url}#foo"
    await navigate_and_assert(bidi_session, frame, url_after)


async def test_navigate_unique_navigation_id(bidi_session, inline, new_tab):
    url = inline("<div>foo</div>")

    result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=url, wait="complete"
    )
    any_string(result["navigation"])

    hash_result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=f"{url}#foo", wait="complete"
    )
    any_string(hash_result["navigation"])
    assert hash_result["navigation"] != result["navigation"]

    other_hash_result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=f"{url}#bar", wait="complete"
    )
    any_string(other_hash_result["navigation"])
    assert other_hash_result["navigation"] != hash_result["navigation"]

    same_hash_result = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=f"{url}#bar", wait="complete"
    )
    any_string(same_hash_result["navigation"])
    assert same_hash_result["navigation"] != other_hash_result["navigation"]


@pytest.mark.parametrize("wait", ["none", "interactive", "complete"])
async def test_navigate_in_iframe_to_current_fragment(
    bidi_session, inline, new_tab, wait
):
    frame_start_url = f"{inline('frame')}#foo"
    url_before = inline(f"<iframe src='{frame_start_url}'></iframe>")
    contexts = await navigate_and_assert(bidi_session, new_tab, url_before)

    assert len(contexts[0]["children"]) == 1
    frame = contexts[0]["children"][0]
    assert frame["url"] == frame_start_url

    # Mark the frame's document so that we can check it is not replaced.
    await bidi_session.script.evaluate(
        expression="document.querySelector('iframe').contentWindow.marker = 'same-document'",
        target=ContextTarget(new_tab["context"]),
        await_promise=False,
    )

    # Navigating to the URL the frame is already at, including its fragment,
    # is a navigation to a fragment and stays in the same document.
    result = await bidi_session.browsing_context.navigate(
        context=frame["context"], url=frame_start_url, wait=wait
    )
    any_string(result["navigation"])
    assert result["url"] == frame_start_url

    contexts = await bidi_session.browsing_context.get_tree(root=frame["context"])
    assert len(contexts) == 1
    assert contexts[0]["url"] == frame_start_url

    result = await bidi_session.script.evaluate(
        expression="document.querySelector('iframe').contentWindow.marker",
        target=ContextTarget(new_tab["context"]),
        await_promise=False,
    )
    assert result == {"type": "string", "value": "same-document"}
