#!/usr/bin/env python3

import sys
from datetime import datetime, timedelta

expires = datetime.utcnow() + timedelta(seconds=300)

sys.stdout.write(
    'status: 200\r\n'
    'Set-Cookie: persistentCookie=1; expires={expires} GMT; Max-Age=300; path=/\r\n'
    'Set-Cookie: persistentSecureCookie=1; expires={expires} GMT; Max-Age=300; path=/; secure\r\n'
    'Set-Cookie: persistentHttpOnlyCookie=1; expires={expires} GMT; Max-Age=300; path=/; HttpOnly\r\n'
    'Set-Cookie: persistentSameSiteLaxCookie=1; expires={expires} GMT; Max-Age=300; path=/; SameSite=Lax\r\n'
    'Set-Cookie: persistentSameSiteStrictCookie=1; expires={expires} GMT; Max-Age=300; path=/; SameSite=Strict\r\n'
    'Set-Cookie: sessionCookie=1; path=/\r\n'
    'Set-Cookie: sessionSecureCookie=1; path=/; secure\r\n'
    'Set-Cookie: sessionHttpOnlyCookie=1; path=/; HttpOnly\r\n'
    'Set-Cookie: sessionSameSiteLaxCookie=1; path=/; SameSite=Lax\r\n'
    'Set-Cookie: sessionSameSiteStrictCookie=1; path=/; SameSite=Strict\r\n'
    'Content-Type: text/html\r\n\r\n'.format(expires=expires.strftime('%a, %d-%b-%Y %H:%M:%S'))
)