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

# aioquic backs the WPT WebTransport-over-HTTP/3 server (tools/webtransport). AutoInstall does not resolve
# a dependency tree, so aioquic's dependencies are declared explicitly, at the versions pip resolves for
# aioquic==1.2.0 (matching tools/webtransport/requirements.txt). cryptography, cffi, OpenSSL, attrs and
# certifi are shared with the rest of webkitpy and already registered in webkitcorepy/__init__.py and
# webkitpy/__init__.py (cryptography/pyOpenSSL were bumped there to satisfy aioquic's cryptography>=42 and
# pyopenssl>=24). They are referenced here via implicit_deps rather than re-registered: AutoInstall enforces
# one version per package name process-wide, so re-registering a different version would raise.
#
# service-identity is pinned to 24.2.0, not the latest: newer releases import cryptography's hazmat.asn1,
# which only exists in cryptography>=44 -- but 44+ dropped prebuilt py3.9 wheels, and webkitpy still supports
# py3.9, so cryptography is capped at 43.0.3. 24.2.0 works against that pin.
#
# aioquic and pylsqpack ship prebuilt wheels for every supported arch, so wheel=True avoids a compile
# toolchain on the bots.

from webkitscmpy import AutoInstall, Package, Version

AutoInstall.install(Package('pylsqpack', Version(0, 3, 22), wheel=True))
AutoInstall.install(Package('pyasn1', Version(0, 6, 4)))
AutoInstall.install(Package('pyasn1_modules', Version(0, 4, 2), pypi_name='pyasn1-modules', implicit_deps=['pyasn1']))
AutoInstall.install(Package('service_identity', Version(24, 2, 0), pypi_name='service-identity', implicit_deps=['attr', 'cryptography', 'OpenSSL', 'pyasn1', 'pyasn1_modules']))
AutoInstall.install(Package('aioquic', Version(1, 2, 0), wheel=True, implicit_deps=['cryptography', 'certifi', 'pylsqpack', 'service_identity']))
