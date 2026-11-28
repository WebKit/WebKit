import pytest
from webdriver.bidi.modules.script import ContextTarget

from . import assert_console_entry

pytestmark = pytest.mark.asyncio


# Browsers can load documents of other sites in other processes, which can be created after subscribing.


async def log_and_get_realm(bidi_session, context, text):
    result = await bidi_session.script.evaluate(
        raw_result=True,
        expression=f"console.log('{text}')",
        await_promise=False,
        target=ContextTarget(context),
    )
    return result["realm"]


async def test_navigate_to_other_site(
    bidi_session, subscribe_events, top_context, wait_for_event, wait_for_future_safe, test_page_cross_origin
):
    await subscribe_events(events=["log.entryAdded"])

    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_cross_origin, wait="complete"
    )

    on_entry_added = wait_for_event("log.entryAdded")
    realm = await log_and_get_realm(bidi_session, top_context["context"], "foo")
    event_data = await wait_for_future_safe(on_entry_added)

    assert_console_entry(
        event_data,
        text="foo",
        args=[{"type": "string", "value": "foo"}],
        realm=realm,
        context=top_context["context"],
    )


async def test_frame_of_other_site(
    bidi_session, subscribe_events, top_context, wait_for_event, wait_for_future_safe, test_page_cross_origin_frame
):
    await subscribe_events(events=["log.entryAdded"])

    await bidi_session.browsing_context.navigate(
        context=top_context["context"], url=test_page_cross_origin_frame, wait="complete"
    )
    contexts = await bidi_session.browsing_context.get_tree(root=top_context["context"])
    assert len(contexts[0]["children"]) == 1
    frame_context = contexts[0]["children"][0]

    on_entry_added = wait_for_event("log.entryAdded")
    realm = await log_and_get_realm(bidi_session, frame_context["context"], "bar")
    event_data = await wait_for_future_safe(on_entry_added)

    assert_console_entry(
        event_data,
        text="bar",
        args=[{"type": "string", "value": "bar"}],
        realm=realm,
        context=frame_context["context"],
    )
