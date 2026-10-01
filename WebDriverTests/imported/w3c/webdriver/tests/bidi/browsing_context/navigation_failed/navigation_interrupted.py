import asyncio
import pytest

from .. import assert_navigation_info

pytestmark = pytest.mark.asyncio

NAVIGATION_ABORTED_EVENT = "browsingContext.navigationAborted"
NAVIGATION_FAILED_EVENT = "browsingContext.navigationFailed"
NAVIGATION_STARTED_EVENT = "browsingContext.navigationStarted"


@pytest.mark.parametrize("second_target", ["fast", "slow"])
async def test_terminal_for_interrupted_navigation_follows_its_start(
    bidi_session, subscribe_events, inline, url, new_tab, second_target
):
    slow_page_url = url(
        "/webdriver/tests/bidi/browsing_context/support/empty.html?pipe=trickle(d10)"
    )
    await subscribe_events(
        events=[NAVIGATION_STARTED_EVENT, NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT]
    )

    events = []

    async def on_event(method, data):
        events.append((method, data))

    remove_listeners = [
        bidi_session.add_event_listener(event, on_event)
        for event in (NAVIGATION_STARTED_EVENT, NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT)
    ]

    # The second command is dispatched before the implementation has necessarily reported the first
    # navigation started; the first navigation must still get a complete, ordered lifecycle.
    first = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=slow_page_url, wait="none"
    )
    if second_target == "fast":
        second_url = inline("<div>second</div>")
    else:
        second_url = f"{slow_page_url}&second=1"
    second = await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=second_url, wait="none"
    )

    def received(method_names, navigation):
        return any(
            method in method_names and data["navigation"] == navigation
            for method, data in events
        )

    # The first navigation's terminal is emitted as soon as the second command supersedes it, which
    # can precede the second navigation's own start; wait for both before inspecting the events.
    for _ in range(50):
        first_terminated = received(
            (NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT), first["navigation"]
        )
        successor_started = received((NAVIGATION_STARTED_EVENT,), second["navigation"])
        if first_terminated and successor_started:
            break
        await asyncio.sleep(0.1)

    for remove in remove_listeners:
        remove()

    first_events = [
        (method, data) for method, data in events if data["navigation"] == first["navigation"]
    ]
    methods = [method for method, _ in first_events]

    assert NAVIGATION_STARTED_EVENT in methods, first_events
    terminals = [m for m in methods if m in (NAVIGATION_ABORTED_EVENT, NAVIGATION_FAILED_EVENT)]
    assert len(terminals) == 1, first_events
    assert methods.index(NAVIGATION_STARTED_EVENT) < methods.index(terminals[0]), first_events

    started = next(data for method, data in first_events if method == NAVIGATION_STARTED_EVENT)
    assert_navigation_info(started, {"context": new_tab["context"], "url": slow_page_url})

    second_started = [
        data for method, data in events
        if method == NAVIGATION_STARTED_EVENT and data["navigation"] == second["navigation"]
    ]
    assert len(second_started) == 1, events
