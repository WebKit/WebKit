# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import io
import json
import os
from collections.abc import Iterable, Mapping
from enum import Enum, auto
from typing import Callable, Dict, IO, List, Optional, Tuple, Union


class Config(dict[str, 'Config.Values']):
    class Mode(Enum):
        JSON = auto()
        YAML = auto()

    CONTEXT_SYMBOL: str = '$context'
    Values = Union[str, int, float, bool, None, List['Config.Values'], Dict[str, 'Config.Values']]

    @classmethod
    def render(cls, value: 'Config.Values', context: Optional[Dict[str, Union['Config.Values', Callable[..., 'Config.Values']]]] = None) -> 'Config.Values':
        import jsone
        from jsone.render import parse as jsone_parse

        def dynamic_pop(jsone_context: Mapping[str, Union['Config.Values', Callable[..., 'Config.Values']]], collection: Union[List['Config.Values'], Mapping[str, 'Config.Values']], *args: Union[int, str]) -> 'Config.Values':
            """Pop an item from a mapping or list which matches some criteria, usable as a jsone built-in.

            For mappings, requires a key argument.

            For lists, accepts an optional index (int) or a jsone filter expression
            (str) that is evaluated against each item to find the first match. Intended
            to allow, for example, a configuration to progressively consume a pre-existing
            list of machines with listed properties.
            """
            if isinstance(collection, list):
                if args and isinstance(args[0], str):
                    filter_expr = args[0]
                    for i, item in enumerate(collection):
                        subcontext = dict(jsone_context)
                        if isinstance(item, Mapping):
                            subcontext.update(item)
                        else:
                            subcontext['it'] = item
                        if jsone_parse(filter_expr, subcontext):
                            return collection.pop(i)
                    raise ValueError(f'No item in list matches filter: {filter_expr!r}')
                return collection.pop(int(args[0])) if args else collection.pop(0)
            if isinstance(collection, Mapping):
                if not args:
                    raise TypeError('pop() on a mapping requires a key argument')
                return collection.pop(args[0])
            raise TypeError('pop() requires a list or mapping')

        # Treat pop as a jsone built-in so we can apply a filter against it
        dynamic_pop._jsone_builtin = True

        context = dict(context or {})
        context.setdefault('pop', dynamic_pop)

        if not isinstance(value, Mapping):
            return jsone.render(value, context=context)

        if cls.CONTEXT_SYMBOL in value:
            result = Config()
            context = dict(**context)
            context.update(cls.render(value[cls.CONTEXT_SYMBOL], context=context))
            for key, content in value.items():
                if key == cls.CONTEXT_SYMBOL:
                    continue
                result[key] = cls.render(content, context=context)
                if isinstance(result[key], list):
                    context[key] = [x for x in result[key]]
                elif isinstance(result[key], Mapping):
                    result[key] = dict(**result[key])
                    context[key] = dict(**result[key])
                else:
                    context[key] = result[key]
            return result

        result = jsone.render(value, context=context)
        if isinstance(result, Mapping):
            return Config(**result)
        return result

    @classmethod
    def loads(cls, string: str, mode: Optional['Config.Mode'] = None) -> 'Config':
        return cls.load(io.StringIO(string), mode=mode)

    @classmethod
    def load(cls, file: IO[str], mode: Optional['Config.Mode'] = None) -> 'Config':
        if mode is None and hasattr(file, 'name'):
            ext = os.path.splitext(file.name)[1].lower()
            if ext == '.json':
                mode = cls.Mode.JSON
            elif ext in ('.yaml', '.yml'):
                mode = cls.Mode.YAML
        if mode is cls.Mode.JSON:
            result = cls.render(json.load(file))
        else:
            import yaml

            # yaml.safe_load_all returns a generator, not a sequence.
            documents = list(yaml.safe_load_all(file))
            if not documents:
                result = cls()
            elif len(documents) == 1:
                if not isinstance(documents[0], dict):
                    raise TypeError('yaml sub-document is not a dictionary')
                result = cls.render(documents[0])
            else:
                data = {cls.CONTEXT_SYMBOL: documents[0]}
                for doc in documents[1:]:
                    if not isinstance(doc, dict):
                        raise TypeError('yaml sub-document is not a dictionary')
                    data.update(doc)
                result = cls.render(data)
            mode = cls.Mode.YAML
        result.mode = mode
        return result

    def __init__(self, mapping: Union[Mapping[str, 'Config.Values'], Iterable[Tuple[str, 'Config.Values']], None] = None, **kwargs: 'Config.Values') -> None:
        super().__init__(**kwargs) if mapping is None else super().__init__(mapping, **kwargs)
        self.mode: 'Config.Mode' = self.Mode.YAML

    def dump(self, file: IO[str], mode: Optional['Config.Mode'] = None, indent: int = 4) -> None:
        mode = mode or self.mode
        if mode is self.Mode.JSON:
            json.dump(dict(self), file, indent=indent)
        else:
            import yaml
            yaml.dump(dict(self), file, indent=indent, default_flow_style=False)

    def dumps(self, mode: Optional['Config.Mode'] = None, indent: int = 4) -> str:
        file = io.StringIO()
        self.dump(file, mode=mode, indent=indent)
        return file.getvalue()
