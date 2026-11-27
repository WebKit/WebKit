import json
from contextlib import suppress

import pytest
import websockets

from tests.support.classic.asserts import assert_success

pytestmark = pytest.mark.asyncio


def close_window(session):
    return session.transport.send(
        "DELETE", "session/{session_id}/window".format(**vars(session)))


def get_status(session):
    return session.transport.send("GET", "/status")


async def close_last_window(session):
    websocket = await websockets.connect(session.capabilities["webSocketUrl"])
    try:
        assert len(session.handles) == 1
        assert_success(close_window(session), [])
    finally:
        # Closing the last window ended the session. Tell the test harness, so
        # that it creates a new session for the next test.
        session.end()
    return websocket


async def close_connection(websocket):
    # The remote end may have closed the connection already, and then closing
    # can fail.
    with suppress(websockets.exceptions.ConnectionClosed):
        await websocket.close()


@pytest.mark.capabilities({"webSocketUrl": True})
async def test_close_connection_after_session_ended(session):
    websocket = await close_last_window(session)

    await close_connection(websocket)

    # The remote end still works.
    assert_success(get_status(session))


@pytest.mark.capabilities({"webSocketUrl": True})
async def test_send_command_after_session_ended(session):
    websocket = await close_last_window(session)

    # The session isn't active anymore, so the command gets no response. The
    # remote end may have closed the connection already, and then sending can
    # fail.
    with suppress(websockets.exceptions.ConnectionClosed):
        await websocket.send(json.dumps({
            "id": 1,
            "method": "session.status",
            "params": {},
        }))
    await close_connection(websocket)

    # The remote end still works.
    assert_success(get_status(session))
