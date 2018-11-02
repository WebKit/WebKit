# Copyright (C) 2018 Sony Interactive Entertainment Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import unittest
import sys

from webkitpy.common.system.pemfile import Pem, BadFormatError


class PemFileTest(unittest.TestCase):
    """Test Pem file parsing"""
    def test_parse(self):
        pem = Pem(self.pem_contents)

        self.assertEqual(pem.private_key, self.private_key)
        self.assertEqual(pem.certificate, self.certificate)

        with self.assertRaises(KeyError):
            pem.csr

        with self.assertRaises(KeyError):
            pem.get("FOO BAR")

    def test_parse_crlf(self):
        pem = Pem(self.pem_contents.replace('\n', '\r\n'))

        self.assertEqual(pem.private_key, self.private_key)
        self.assertEqual(pem.certificate, self.certificate)

    def test_parse_cr(self):
        pem = Pem(self.pem_contents.replace('\n', '\r'))

        self.assertEqual(pem.private_key, self.private_key)
        self.assertEqual(pem.certificate, self.certificate)

    def test_parse_bad_format(self):
        with self.assertRaises(BadFormatError):
            # Partial contents raises format error
            Pem(trim("""-----BEGIN PRIVATE KEY-----
            MIICXQIBAAKBgQCmcXbusrr8zQr8snIb0OVQibVfgv7zPjh/5xdcrKOejJzp3epA
            AF4TITeFR9vzWIwkmkcRoY+IbQNhh7kefGUYD47bvVamJMtq5cGYVs0HngT+KTMa
            NGH/G44KkFIOaz/b5d/JNKONrlqwxqXS+m6IY4l/E1Ff25ZjND5TaEvI1wIDAQAB
            """) + "\n")

        with self.assertRaises(BadFormatError):
            # No section content raises format error
            Pem("""HELLO WORLD
            HOW'S GOING????
            """)

    def test_parse_custom_content(self):
        pem = Pem(trim("""Hi, please take this.
        -----BEGIN FOOBAR-----
        HELLO/WORKD===
        -----END FOOBAR-----
        regards,
        kind of mail signature comes here
        """) + "\n")

        self.assertEqual(
            trim(pem.get("FOOBAR")),
            trim("""-----BEGIN FOOBAR-----
                    HELLO/WORKD===
                    -----END FOOBAR-----"""))

    def setUp(self):
        self.pem_contents = trim("""-----BEGIN PRIVATE KEY-----
        MIICXQIBAAKBgQCmcXbusrr8zQr8snIb0OVQibVfgv7zPjh/5xdcrKOejJzp3epA
        AF4TITeFR9vzWIwkmkcRoY+IbQNhh7kefGUYD47bvVamJMtq5cGYVs0HngT+KTMa
        NGH/G44KkFIOaz/b5d/JNKONrlqwxqXS+m6IY4l/E1Ff25ZjND5TaEvI1wIDAQAB
        AoGBAIcDv4A9h6UOBv2ZGyspNvsv2erSblGOhXJrWO4aNNemJJspIp4sLiPCbDE3
        a1po17XRWBkbPz1hgL6axDXQnoeo++ebfrvRSed+Fys4+6SvuSrPOv6PmWTBT/Wa
        GpO+tv48JUNxC/Dy8ROixBXOViuIBEFq3NfVH4HU3+RG20NhAkEA1L3RAhdfPkLI
        82luSOYE3Eq44lICb/yZi+JEihwSeZTJKdZHwYD8KVCjOtjGrOmyEyvThrcIACQz
        JLEreVh33wJBAMhJm9pzJJNkIyBgiXA66FAwbhdDzSTPx0OBjoVWoj6u7jzGvIFT
        Cn1aiTBYzzsiMCaCx+W3e6pK/DcvHSwKrgkCQHZMcxwBmSHLC2lnmD8LQWqqVnLr
        fZV+VnfVw501DQT0uoP8NvygWBg1Uf9YKepfLXnBpidEQjup5ZKivnUEv+sCQA8N
        6VcMHI2vkyxV1T7ITrnoSf4ZrIu9yl56mHnRPzSy9VlAHt8hnMI7UeB+bGUndrMO
        VXQgzHzKUhbbxbePvfECQQDTtkOuhJyKDfHCxLDcwNpi+T6OWTEfCw/cq9ZWDbA7
        yCX81pQxfZkfMIS1YFIOGHovK0rMMTraCe+iDNYtVz/L
        -----END PRIVATE KEY-----
        -----BEGIN CERTIFICATE-----
        MIIB9zCCAWACCQDjWWTeC6BQvTANBgkqhkiG9w0BAQQFADBAMQswCQYDVQQGEwJB
        VTETMBEGA1UECBMKU29tZS1TdGF0ZTEcMBoGA1UEChMTV2ViS2l0IExheW91dCBU
        ZXN0czAeFw0wNzA3MTMxMjUxMzJaFw03MTA1MTMwNjIzMTZaMEAxCzAJBgNVBAYT
        AkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRwwGgYDVQQKExNXZWJLaXQgTGF5b3V0
        IFRlc3RzMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCmcXbusrr8zQr8snIb
        0OVQibVfgv7zPjh/5xdcrKOejJzp3epAAF4TITeFR9vzWIwkmkcRoY+IbQNhh7ke
        fGUYD47bvVamJMtq5cGYVs0HngT+KTMaNGH/G44KkFIOaz/b5d/JNKONrlqwxqXS
        +m6IY4l/E1Ff25ZjND5TaEvI1wIDAQABMA0GCSqGSIb3DQEBBAUAA4GBAAfbUbgD
        01O8DoZA02c1MUMbMHRPSb/qdok2pyWoCPa/BSaOIaNPePc8auPRbrS2XsVWSMft
        CTXiXmrK2Xx1+fJuZLAp0CUng4De4cDH5c8nvlocYmXo+1x53S9DfD0KPryjBRI7
        9LnJq2ysHAUawiqFXlwBag6mXawD8YjzcYat
        -----END CERTIFICATE-----
        """) + "\n"

        self.private_key = trim("""-----BEGIN PRIVATE KEY-----
        MIICXQIBAAKBgQCmcXbusrr8zQr8snIb0OVQibVfgv7zPjh/5xdcrKOejJzp3epA
        AF4TITeFR9vzWIwkmkcRoY+IbQNhh7kefGUYD47bvVamJMtq5cGYVs0HngT+KTMa
        NGH/G44KkFIOaz/b5d/JNKONrlqwxqXS+m6IY4l/E1Ff25ZjND5TaEvI1wIDAQAB
        AoGBAIcDv4A9h6UOBv2ZGyspNvsv2erSblGOhXJrWO4aNNemJJspIp4sLiPCbDE3
        a1po17XRWBkbPz1hgL6axDXQnoeo++ebfrvRSed+Fys4+6SvuSrPOv6PmWTBT/Wa
        GpO+tv48JUNxC/Dy8ROixBXOViuIBEFq3NfVH4HU3+RG20NhAkEA1L3RAhdfPkLI
        82luSOYE3Eq44lICb/yZi+JEihwSeZTJKdZHwYD8KVCjOtjGrOmyEyvThrcIACQz
        JLEreVh33wJBAMhJm9pzJJNkIyBgiXA66FAwbhdDzSTPx0OBjoVWoj6u7jzGvIFT
        Cn1aiTBYzzsiMCaCx+W3e6pK/DcvHSwKrgkCQHZMcxwBmSHLC2lnmD8LQWqqVnLr
        fZV+VnfVw501DQT0uoP8NvygWBg1Uf9YKepfLXnBpidEQjup5ZKivnUEv+sCQA8N
        6VcMHI2vkyxV1T7ITrnoSf4ZrIu9yl56mHnRPzSy9VlAHt8hnMI7UeB+bGUndrMO
        VXQgzHzKUhbbxbePvfECQQDTtkOuhJyKDfHCxLDcwNpi+T6OWTEfCw/cq9ZWDbA7
        yCX81pQxfZkfMIS1YFIOGHovK0rMMTraCe+iDNYtVz/L
        -----END PRIVATE KEY-----
        """) + "\n"

        self.certificate = trim("""-----BEGIN CERTIFICATE-----
        MIIB9zCCAWACCQDjWWTeC6BQvTANBgkqhkiG9w0BAQQFADBAMQswCQYDVQQGEwJB
        VTETMBEGA1UECBMKU29tZS1TdGF0ZTEcMBoGA1UEChMTV2ViS2l0IExheW91dCBU
        ZXN0czAeFw0wNzA3MTMxMjUxMzJaFw03MTA1MTMwNjIzMTZaMEAxCzAJBgNVBAYT
        AkFVMRMwEQYDVQQIEwpTb21lLVN0YXRlMRwwGgYDVQQKExNXZWJLaXQgTGF5b3V0
        IFRlc3RzMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCmcXbusrr8zQr8snIb
        0OVQibVfgv7zPjh/5xdcrKOejJzp3epAAF4TITeFR9vzWIwkmkcRoY+IbQNhh7ke
        fGUYD47bvVamJMtq5cGYVs0HngT+KTMaNGH/G44KkFIOaz/b5d/JNKONrlqwxqXS
        +m6IY4l/E1Ff25ZjND5TaEvI1wIDAQABMA0GCSqGSIb3DQEBBAUAA4GBAAfbUbgD
        01O8DoZA02c1MUMbMHRPSb/qdok2pyWoCPa/BSaOIaNPePc8auPRbrS2XsVWSMft
        CTXiXmrK2Xx1+fJuZLAp0CUng4De4cDH5c8nvlocYmXo+1x53S9DfD0KPryjBRI7
        9LnJq2ysHAUawiqFXlwBag6mXawD8YjzcYat
        -----END CERTIFICATE-----
        """) + "\n"


# Formal docstring indentation handling from PEP
# https://www.python.org/dev/peps/pep-0257/
def trim(docstring):
    if not docstring:
        return ''
    # Convert tabs to spaces (following the normal Python rules)
    # and split into a list of lines:
    lines = docstring.expandtabs().splitlines()
    # Determine minimum indentation (first line doesn't count):
    indent = sys.maxint
    for line in lines[1:]:
        stripped = line.lstrip()
        if stripped:
            indent = min(indent, len(line) - len(stripped))
    # Remove indentation (first line is special):
    trimmed = [lines[0].strip()]
    if indent < sys.maxint:
        for line in lines[1:]:
            trimmed.append(line[indent:].rstrip())
    # Strip off trailing and leading blank lines:
    while trimmed and not trimmed[-1]:
        trimmed.pop()
    while trimmed and not trimmed[0]:
        trimmed.pop(0)
    # Return a single string:
    return '\n'.join(trimmed)
