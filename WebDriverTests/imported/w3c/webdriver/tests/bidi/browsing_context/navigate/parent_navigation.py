import asyncio

import pytest
from webdriver.bidi.error import UnknownErrorException

pytestmark = pytest.mark.asyncio

NAVIGATION_STARTED_EVENT = "browsingContext.navigationStarted"


@pytest.mark.parametrize("domain", ["", "alt"], ids=["same_origin", "cross_origin"])
async def test_navigate_parent_away(
    bidi_session,
    subscribe_events,
    inline,
    url,
    new_tab,
    test_page,
    wait_for_event,
    wait_for_future_safe,
    iframe,
    domain,
):
    page_url = inline(iframe("<div>foo</div>", domain=domain))

    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=page_url, wait="complete"
    )

    contexts = await bidi_session.browsing_context.get_tree(root=new_tab["context"])
    iframe_context = contexts[0]["children"][0]["context"]

    slow_page_url = url(
        "/webdriver/tests/bidi/browsing_context/support/empty.html?pipe=trickle(d10)",
        domain=domain,
    )
    await subscribe_events(events=[NAVIGATION_STARTED_EVENT])

    on_navigation_started = wait_for_event(NAVIGATION_STARTED_EVENT)
    # Navigate in the iframe.
    task = asyncio.ensure_future(
        bidi_session.browsing_context.navigate(
            context=iframe_context, url=slow_page_url, wait="complete"
        )
    )
    await wait_for_future_safe(on_navigation_started)

    # Navigate the top context to another document. Unlike a reload, this can
    # put the old document into the back/forward cache with its child
    # navigables, which then are not destroyed.
    await bidi_session.browsing_context.navigate(
        context=new_tab["context"], url=test_page, wait="complete"
    )

    # Make sure that the iframe navigation failed.
    with pytest.raises(UnknownErrorException):
        await wait_for_future_safe(task)
