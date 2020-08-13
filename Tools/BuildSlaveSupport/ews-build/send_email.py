# Copyright (C) 2020 Apple Inc. All rights reserved.
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

import os
import smtplib

from email.mime.text import MIMEText

is_test_mode_enabled = os.getenv('BUILDBOT_PRODUCTION') is None

BOT_WATCHERS_EMAILS = ['webkit-ews-bot-watchers@group.apple.com']
FROM_EMAIL = 'ews@webkit.org'
SERVER = 'localhost'


def send_email(to_emails, subject, text):
    if is_test_mode_enabled:
        return
    if not to_emails:
        print('Error: skipping email since no recipient is specified')
        return
    if not subject or not text:
        print('Error: skipping email since no subject or text is specified')
        return

    text = text.encode('utf-8')
    text = text.replace('\n', '<br>')

    msg = MIMEText(text, 'html')
    msg['From'] = FROM_EMAIL
    msg['To'] = ', '.join(to_emails)
    msg['Subject'] = subject

    server = smtplib.SMTP(SERVER)
    server.sendmail(FROM_EMAIL, to_emails, msg.as_string())
    server.quit()


def send_email_to_bot_watchers(subject, text):
    send_email(BOT_WATCHERS_EMAILS, subject, text)
