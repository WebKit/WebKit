# Copyright (C) 2026 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Runs the web-platform-tests linter against changed WPT files."""

import importlib.util
import inspect
import os

from webkitpy.layout_tests.controllers.layout_test_finder import IMPORTED_WPT_DIR
from webkitpy.w3c.wpt_linter import WPTLinter


class WPTLintRulesImportError(Exception):
    """Raised when WPT's tools/lint/rules.py is present but cannot be loaded.

    Loud-fail so that upstream WPT changes (e.g. new non-stdlib imports in
    rules.py) are noticed promptly rather than silently dropping the per-rule
    subcategories from check-webkit-style.
    """


def _load_wpt_lint_rules_module():
    """Return the executed rules.py module, or None if the file is absent.

    Raises WPTLintRulesImportError if the file is present but fails to load.
    """
    rules_path = os.path.join('LayoutTests', *IMPORTED_WPT_DIR.split('/'), 'tools', 'lint', 'rules.py')
    if not os.path.exists(rules_path):
        return None
    try:
        spec = importlib.util.spec_from_file_location('_wpt_lint_rules', rules_path)
        if spec is None or spec.loader is None:
            raise WPTLintRulesImportError('Unable to create import spec for %s' % rules_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    except WPTLintRulesImportError:
        raise
    except Exception as e:
        raise WPTLintRulesImportError(
            'Failed to load WPT lint rules from %s: %s' % (rules_path, e)
        ) from e


def _normalize_rule_name(rule):
    """Normalize WPT rule names to webkitpy.style categories.

    For example "WORKER COLLISION" is transformed to "worker_collision".
    """
    return rule.lower().replace(' ', '_').replace('-', '_')


def wpt_lint_categories():
    """Return ``wpt/lint/<rule>`` categories derived from WPT's rules.py."""
    categories = {'wpt/lint'}
    module = _load_wpt_lint_rules_module()
    if module is None:
        return categories
    for obj in vars(module).values():
        if inspect.isclass(obj) and isinstance(getattr(obj, 'name', None), str):
            categories.add('wpt/lint/' + _normalize_rule_name(obj.name))
    return categories


def run_wpt_lint(wpt_repo_dir, wpt_paths, configuration, increment_error_count):
    """Run the WPT linter and report each error via the style configuration."""
    for error in WPTLinter(wpt_repo_dir).lint(wpt_paths):
        file_path = os.path.join('LayoutTests', *IMPORTED_WPT_DIR.split('/'), error['path'])
        line_number = error['lineno'] if error['lineno'] is not None else '-'
        category = 'wpt/lint/' + _normalize_rule_name(error['rule'])
        if configuration.is_reportable(category=category, confidence_in_error=5, file_path=file_path):
            configuration.write_style_error(
                category=category,
                confidence_in_error=5,
                file_path=file_path,
                line_number=line_number,
                message='[%s] %s' % (error['rule'], error['message']),
            )
            increment_error_count()
