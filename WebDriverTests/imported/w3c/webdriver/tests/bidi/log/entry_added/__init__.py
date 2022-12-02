from webdriver.bidi.modules.script import ContextTarget


def assert_base_entry(
    entry,
    level=None,
    text=None,
    time_start=None,
    time_end=None,
    stacktrace=None,
    realm=None,
    context=None,
):
    assert "level" in entry
    assert isinstance(entry["level"], str)
    if level is not None:
        assert entry["level"] == level

    assert "text" in entry
    assert isinstance(entry["text"], str)
    if text is not None:
        assert entry["text"] == text

    assert "timestamp" in entry
    assert isinstance(entry["timestamp"], int)
    if time_start is not None:
        assert entry["timestamp"] >= time_start
    if time_end is not None:
        assert entry["timestamp"] <= time_end

    if stacktrace is not None:
        assert "stackTrace" in entry
        assert isinstance(entry["stackTrace"], object)
        assert "callFrames" in entry["stackTrace"]

        call_frames = entry["stackTrace"]["callFrames"]
        assert isinstance(call_frames, list)
        assert len(call_frames) == len(stacktrace)
        for index in range(0, len(call_frames)):
            assert call_frames[index] == stacktrace[index]

    assert "source" in entry
    source = entry["source"]

    assert "realm" in source
    assert isinstance(source["realm"], str)

    if realm is not None:
        assert source["realm"] == realm

    if context is not None:
        assert "context" in source
        assert source["context"] == context


def assert_console_entry(
    entry,
    method=None,
    level=None,
    text=None,
    args=None,
    time_start=None,
    time_end=None,
    realm=None,
    stacktrace=None,
    context=None,
):
    assert_base_entry(
        entry, level, text, time_start, time_end, stacktrace, realm, context
    )

    assert "type" in entry
    assert isinstance(entry["type"], str)
    assert entry["type"] == "console"

    assert "method" in entry
    assert isinstance(entry["method"], str)
    if method is not None:
        assert entry["method"] == method

    assert "args" in entry
    assert isinstance(entry["args"], list)
    if args is not None:
        assert entry["args"] == args


def assert_javascript_entry(
    entry,
    level=None,
    text=None,
    time_start=None,
    time_end=None,
    stacktrace=None,
    realm=None,
    context=None,
):
    assert_base_entry(
        entry, level, text, time_start, time_end, stacktrace, realm, context
    )

    assert "type" in entry
    assert isinstance(entry["type"], str)
    assert entry["type"] == "javascript"


async def create_console_api_message(bidi_session, context, text):
    await bidi_session.script.call_function(
        function_declaration="""(text) => console.log(text)""",
        arguments=[{"type": "string", "value": text}],
        await_promise=False,
        target=ContextTarget(context["context"]),
    )
    return text


async def create_console_api_message_for_primitive_value(bidi_session, context, type, value):
    await bidi_session.script.evaluate(
        expression=f"""console.{type}({value})""",
        await_promise=False,
        target=ContextTarget(context["context"]),
    )


async def create_javascript_error(bidi_session, context, error_message="foo"):
    str_remote_value = {"type": "string", "value": error_message}

    result = await bidi_session.script.call_function(
        function_declaration="""(error_message) => {
            const script = document.createElement("script");
            script.append(document.createTextNode(`(() => { throw new Error("${error_message}") })()`));
            document.body.append(script);

            const err = new Error(error_message);
            return err.toString();
        }""",
        arguments=[str_remote_value],
        await_promise=False,
        target=ContextTarget(context["context"]),
    )

    return result["value"]


def create_log(bidi_session, context, log_type, text="foo"):
    if log_type == "console_api_log":
        return create_console_api_message(bidi_session, context, text)
    if log_type == "javascript_error":
        return create_javascript_error(bidi_session, context, text)
