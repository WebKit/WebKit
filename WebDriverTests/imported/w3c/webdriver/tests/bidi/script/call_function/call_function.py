import pytest

from webdriver.bidi.modules.script import ContextTarget, ScriptEvaluateResultException
from ... import recursive_compare, any_string, any_int
from .. import any_stack_trace


@pytest.mark.asyncio
async def test_arrow_function(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="()=>{return 1+2;}",
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    assert result == {
        "type": "number",
        "value": 3}


@pytest.mark.asyncio
async def test_arguments(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="(...args)=>{return args}",
        arguments=[{
            "type": "string",
            "value": "ARGUMENT_STRING_VALUE"
        }, {
            "type": "number",
            "value": 42}],
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    recursive_compare({
        "type": "array",
        "value": [{
            "type": 'string',
            "value": 'ARGUMENT_STRING_VALUE'
        }, {
            "type": 'number',
            "value": 42}]},
        result)


@pytest.mark.asyncio
async def test_default_arguments(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="(...args)=>{return args}",
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    recursive_compare({
        "type": "array",
        "value": []
    }, result)


@pytest.mark.asyncio
async def test_this(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="function(){return this.some_property}",
        this={
            "type": "object",
            "value": [[
                "some_property",
                {
                    "type": "number",
                    "value": 42
                }]]},
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    assert result == {
        'type': 'number',
        'value': 42}


@pytest.mark.asyncio
async def test_default_this(bidi_session, top_context):
    result = await bidi_session.script.call_function(
        function_declaration="function(){return this}",
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    # Note: https://github.com/w3c/webdriver-bidi/issues/251
    recursive_compare({
        "type": 'window',
    }, result)


@pytest.mark.asyncio
async def test_remote_value_argument(bidi_session, top_context):
    remote_value_result = await bidi_session.script.evaluate(
        expression="({SOME_PROPERTY:'SOME_VALUE'})",
        await_promise=False,
        result_ownership="root",
        target=ContextTarget(top_context["context"]))

    remote_value_handle = remote_value_result["handle"]

    result = await bidi_session.script.call_function(
        function_declaration="(obj)=>{return obj.SOME_PROPERTY;}",
        arguments=[{
            "handle": remote_value_handle}],
        await_promise=False,
        target=ContextTarget(top_context["context"]))

    assert result == {
        "type": "string",
        "value": "SOME_VALUE"}
