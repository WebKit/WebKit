# Copyright (C) 2026 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

import json
import logging
import os
import shutil

from webkitpy.common.host import Host
from webkitpy.style.error_handlers import DefaultStyleErrorHandler


_log = logging.getLogger(__name__)

WEB_INSPECTOR_UI_DIRECTORY = os.path.join('Source', 'WebInspectorUI')
WEB_INSPECTOR_UI_ESLINT_CONFIGURATION = os.path.join(WEB_INSPECTOR_UI_DIRECTORY, 'eslint.config.mjs')


class ESLintChecker(object):
    categories = set(['js/eslint'])

    @staticmethod
    def check(files, configuration, cwd, increment_error_count=lambda: 0, host=Host()):
        web_inspector_ui_directory = os.path.join(cwd, WEB_INSPECTOR_UI_DIRECTORY)
        eslint_configuration_changed = False
        eslint_paths = []

        for absolute_path in files:
            relative_path = os.path.relpath(absolute_path, cwd)
            if relative_path == WEB_INSPECTOR_UI_ESLINT_CONFIGURATION:
                eslint_configuration_changed = True
                continue
            if not relative_path.startswith(WEB_INSPECTOR_UI_DIRECTORY + os.sep):
                continue
            if not relative_path.endswith('.js') or not host.filesystem.exists(absolute_path):
                continue
            eslint_paths.append(os.path.relpath(absolute_path, web_inspector_ui_directory))

        if not eslint_configuration_changed and not eslint_paths:
            return

        eslint = shutil.which('eslint')
        if not eslint:
            _log.info('ESLint was not found in PATH; skipping WebInspectorUI lint.')
            return

        command = [eslint, '--format=json', '--no-warn-ignored']
        command.extend(['.'] if eslint_configuration_changed else sorted(set(eslint_paths)))

        try:
            output = host.executive.run_command(
                command,
                cwd=web_inspector_ui_directory,
                decode_output=True,
                ignore_errors=True,
                return_stderr=False,
            )
            results = json.loads(output)
            if not isinstance(results, list):
                raise ValueError
        except (OSError, ValueError, json.JSONDecodeError):
            style_error_handler = DefaultStyleErrorHandler(
                WEB_INSPECTOR_UI_ESLINT_CONFIGURATION, configuration, increment_error_count)
            style_error_handler(0, 'js/eslint', 5, 'ESLint failed to produce valid JSON output.')
            return

        for result in results:
            result_path = result.get('filePath', WEB_INSPECTOR_UI_ESLINT_CONFIGURATION)
            if not os.path.isabs(result_path):
                result_path = os.path.join(web_inspector_ui_directory, result_path)
            result_path = os.path.normpath(result_path)
            relative_result_path = os.path.relpath(result_path, cwd)
            line_numbers = None if eslint_configuration_changed else files.get(result_path, [])
            style_error_handler = DefaultStyleErrorHandler(
                relative_result_path, configuration, increment_error_count, line_numbers)

            for message in result.get('messages', []):
                if message.get('severity') != 2:
                    continue
                rule = message.get('ruleId')
                description = '[{}] {}'.format(rule, message['message']) if rule else message['message']
                style_error_handler(message.get('line') or 0, 'js/eslint', 5, description)
