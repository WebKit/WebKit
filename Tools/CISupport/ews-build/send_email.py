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

import json
import os
import smtplib
import socket

from email.mime.text import MIMEText

custom_suffix = '-uat' if os.getenv('BUILDBOT_UAT') else ''

CURRENT_HOSTNAME = socket.gethostname().strip()
EWS_BUILD_HOSTNAME = 'ews-build.webkit.org'
FROM_EMAIL = 'ews@webkit{}.org'.format(custom_suffix)
IGALIA_JSC_QUEUES_PATTERNS = ['armv7', 'mips', 'i386']
IGALIA_GTK_WPE_QUEUES_PATTERNS = ['gtk', 'wpe']
SERVER = 'localhost'


def get_email_ids(category):
    # Valid categories: 'ADMIN_EMAILS', 'APPLE_BOT_WATCHERS_EMAILS', 'EMAIL_IDS_TO_UNSUBSCRIBE' etc.
    try:
        emails = json.load(open('emails.json'))
        return emails.get(category, [])
    except Exception as e:
        print('Error in reading emails.json: {}'.format(e))
        return []


def send_email(to_emails, subject, text, reference=''):
    if CURRENT_HOSTNAME != EWS_BUILD_HOSTNAME:
        # Only allow EWS production instance to send emails.
        return
    if not to_emails:
        print('Error: skipping email since no recipient is specified')
        return
    if not subject or not text:
        print('Error: skipping email since no subject or text is specified')
        return

    text = text.replace('\n', '<br>')

    msg = MIMEText(text, 'html')
    msg['From'] = FROM_EMAIL
    msg['To'] = ', '.join(to_emails)
    msg['Subject'] = subject
    msg.add_header('reply-to', 'aakash_jain@apple.com')
    if reference:
        msg.add_header('references', '{}@webkit.org'.format(reference))

    server = smtplib.SMTP(SERVER)
    server.sendmail(FROM_EMAIL, to_emails, msg.as_string())
    server.quit()


def send_email_to_patch_author(author_email, subject, text, reference=''):
    if not author_email:
        return
    if author_email in get_email_ids('EMAIL_IDS_TO_UNSUBSCRIBE'):
        print('email {} is in unsubscribe list, skipping email'.format(author_email))
        return
    send_email([author_email], subject, text, reference)


def send_email_to_bot_watchers(subject, text, builder_name, reference=''):
    if any(pattern in builder_name.lower() for pattern in IGALIA_JSC_QUEUES_PATTERNS):
        send_email(get_email_ids('IGALIA_JSC_TEAM_EMAILS'), subject, text, reference)
    elif any(pattern in builder_name.lower() for pattern in IGALIA_GTK_WPE_QUEUES_PATTERNS):
        send_email(get_email_ids('IGALIA_GTK_WPE_EMAILS'), subject, text, reference)
    else:
        send_email(get_email_ids('APPLE_BOT_WATCHERS_EMAILS'), subject, text, reference)


def send_email_to_github_admin(subject, text, reference=''):
    send_email(get_email_ids('GITHUB_ADMIN_EMAILS'), subject, text, reference)
