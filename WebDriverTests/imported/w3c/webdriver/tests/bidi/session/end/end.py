import asyncio
import copy
from urllib.parse import urljoin

import pytest
import webdriver

from tests.support.classic.asserts import assert_error
from webdriver.bidi.client import BidiSession
from webdriver.bidi.error import InvalidSessionIDError
from webdriver.transport import HTTPWireProtocol


pytestmark = pytest.mark.asyncio


async def test_end(bidi_session):
    result = await bidi_session.session.end()

    assert result == {}


async def test_end_closes_all_connections(bidi_session):
    primary_transport = bidi_session.transport
    assert primary_transport is not None

    additional_session = BidiSession(
        bidi_session.websocket_url,
        session_id=bidi_session.session_id,
        capabilities=bidi_session.capabilities,
    )
    await additional_session.start()
    additional_transport = additional_session.transport
    assert additional_transport is not None

    try:
        # Use the low-level command API so the session.end result hook does not
        # implicitly wait for the primary connection to close.
        end_result = await bidi_session.send_command("session.end", {})

        assert await end_result == {}
        await asyncio.wait_for(
            asyncio.gather(
                primary_transport.wait_closed(),
                additional_transport.wait_closed(),
            ),
            timeout=2,
        )
    finally:
        await additional_session.end()


async def test_end_invalidates_classic_session(bidi_session, configuration):
    session_id = bidi_session.session_id
    assert session_id is not None
    assert await bidi_session.session.end() == {}

    transport = HTTPWireProtocol(configuration["host"], configuration["port"])
    try:
        response = await asyncio.get_running_loop().run_in_executor(
            None, transport.send, "GET", f"session/{session_id}/url"
        )
        assert_error(response, "invalid session id")
    finally:
        transport.close()


async def test_end_on_static_connection(bidi_session, wait_for_future_safe):
    static_url = urljoin(bidi_session.websocket_url, "/session")
    static_session = BidiSession.bidi_only(static_url)
    try:
        await static_session.start_transport()
        response = await static_session.send_command("session.end", {})
        with pytest.raises(InvalidSessionIDError):
            await wait_for_future_safe(response)
    finally:
        await static_session.end()

    assert await bidi_session.browsing_context.get_tree()


async def test_end_allows_replacement_session_after_success(
    bidi_session, configuration
):
    ended_session_transport = bidi_session.transport
    assert ended_session_transport is not None

    # Receive the success response without waiting for the ended session's
    # WebSocket connection to close.
    end_response = await bidi_session.send_command("session.end", {})
    assert await end_response == {}

    replacement_session = webdriver.Session(
        configuration["host"],
        configuration["port"],
        capabilities={
            "alwaysMatch": copy.deepcopy(configuration["capabilities"])
        },
        enable_bidi=True,
    )
    replacement_bidi_session = None
    event_loop = asyncio.get_running_loop()

    try:
        # Start the replacement immediately after the response and before
        # waiting for the ended session's asynchronous cleanup. Cleanup may
        # therefore overlap replacement-session creation, but is not required
        # to do so for this test to pass.
        await event_loop.run_in_executor(None, replacement_session.start)
        replacement_bidi_session = replacement_session.bidi_session
        assert replacement_bidi_session is not None
        await replacement_bidi_session.start()

        # Observe the ended session's connection closing, then verify that the
        # replacement session remains usable.
        await asyncio.wait_for(
            ended_session_transport.wait_closed(), timeout=5
        )

        contexts = await replacement_bidi_session.browsing_context.get_tree()
        assert contexts
    finally:
        await event_loop.run_in_executor(None, replacement_session.end)
        if replacement_bidi_session is not None:
            await replacement_bidi_session.end()
