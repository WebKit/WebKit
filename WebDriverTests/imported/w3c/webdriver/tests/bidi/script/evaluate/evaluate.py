import pytest
from webdriver.bidi.modules.script import ContextTarget


@pytest.mark.asyncio
async def test_eval(bidi_session, top_context):
    result = await bidi_session.script.evaluate(
        expression="1 + 2",
        target=ContextTarget(top_context["context"]),
        await_promise=True)

    assert result == {
        "type": "number",
        "value": 3}


@pytest.mark.asyncio
async def test_interact_with_dom(bidi_session, top_context):
    result = await bidi_session.script.evaluate(
        expression="'window.location.href: ' + window.location.href",
        target=ContextTarget(top_context["context"]),
        await_promise=True)

    assert result == {
        "type": "string",
        "value": "window.location.href: about:blank"}
