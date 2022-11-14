import pytest

from webdriver.bidi.modules.script import ContextTarget
from ... import recursive_compare


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "argument, expected",
    [
        ({"type": "undefined"}, "undefined"),
        ({"type": "null"}, "null"),
        ({"type": "string", "value": "foobar"}, "'foobar'"),
        ({"type": "string", "value": "2"}, "'2'"),
        ({"type": "number", "value": "-0"}, "-0"),
        ({"type": "number", "value": "Infinity"}, "Infinity"),
        ({"type": "number", "value": "-Infinity"}, "-Infinity"),
        ({"type": "number", "value": 3}, "3"),
        ({"type": "number", "value": 1.4}, "1.4"),
        ({"type": "boolean", "value": True}, "true"),
        ({"type": "boolean", "value": False}, "false"),
        ({"type": "bigint", "value": "42"}, "42n"),
    ],
)
async def test_primitive_values(bidi_session, top_context, argument, expected):
    result = await bidi_session.script.call_function(
        function_declaration=
        f"""(arg) => {{
            if(arg!=={expected})
                throw Error("Argument should be {expected}, but was "+arg);
            return arg;
        }}""",
        arguments=[argument],
        await_promise=False,
        target=ContextTarget(top_context["context"]),
    )

    recursive_compare(argument, result)


@pytest.mark.asyncio
async def test_nan(bidi_session, top_context):
    nan_remote_value = {"type": "number", "value": "NaN"}
    result = await bidi_session.script.call_function(
        function_declaration=
        f"""(arg) => {{
            if(!isNaN(arg))
                throw Error("Argument should be 'NaN', but was "+arg);
            return arg;
        }}""",
        arguments=[nan_remote_value],
        await_promise=False,
        target=ContextTarget(top_context["context"]),
    )

    recursive_compare(nan_remote_value, result)


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "argument, expected_type",
    [
        ({
             "type": "array",
             "value": [
                 {"type": "string", "value": "foobar"},
             ],
         },
         "Array"
        ),
        ({"type": "date", "value": "2022-05-31T13:47:29.000Z"},
         "Date"
         ),
        ({
             "type": "map",
             "value": [
                 ["foobar", {"type": "string", "value": "foobar"}],
             ],
         },
         "Map"
        ),
        ({
             "type": "object",
             "value": [
                 ["foobar", {"type": "string", "value": "foobar"}],
             ],
         },
         "Object"
        ),
        ({"type": "regexp", "value": {"pattern": "foo", "flags": "g"}},
         "RegExp"
         ),
        ({
             "type": "set",
             "value": [
                 {"type": "string", "value": "foobar"},
             ],
         },
         "Set"
        )
    ],
)
async def test_local_values(bidi_session, top_context, argument, expected_type):
    result = await bidi_session.script.call_function(
        function_declaration=
        f"""(arg) => {{
            if(! (arg instanceof {expected_type}))
                throw Error("Argument type should be {expected_type}, but was "+
                    Object.prototype.toString.call(arg));
            return arg;
        }}""",
        arguments=[argument],
        await_promise=False,
        target=ContextTarget(top_context["context"]),
    )

    recursive_compare(argument, result)
