#!/usr/bin/env python3

# Copyright (C) 2022 Apple Inc. All rights reserved.
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
# 3.  Neither the name of Apple Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
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

from keyserver.CKCContainer import *
from keyserver.Constants import *
from keyserver.DFunction import *
from keyserver.SPCContainer import *
from keyserver.Structs import *
from keyserver.TLLVs import *
from keyserver.Utils import *

privateKey = Utils.LoadPrivateKey(b"""-----BEGIN RSA PRIVATE KEY-----
MIICXAIBAAKBgQDKOSWKpii2sqlJeg0TRHZJF3aocb4j8K7RDmmii2vwrcv2ECiR
TnGZp3/giNyj74Ty0/hjf71ck7ldJryHi7iO5dUVjatjmF5giRdyjAPp9dVxj2nM
DyQORcCPzGgDTJ8BNt2oM1tOant+/3J9jDcWw3vjWeZURX7QH5+gpQuC0QIDAQAB
AoGBAMYAiGKmaMziSGE6UR0hdpQAll096Ye1POZTL1lJHCovcbp/fckwvohNeic2
upsFhe5hYB7ET3wa5G9x4zzwsIX0sLXfsaNBYCHhneoE/x5rsx0Vqa2ngcufL82l
hbKaam9i29kCbilx8TMyIG7k8nYIF9pjZWNIhzHH92/WHzNFAkEA5D+RyyYCHJky
olJqIimEm4nihmgG+PGlAVJIlUw2fZdH/1YYKQgUDr4mb5FSbA7NjNPdCiSyyRNE
QlSjiAKhzwJBAOLPhfPTJMaWTVPUmu5Sn/BRHe5FPQqMxKFP0sHqq1Uo6qlCohl6
+8VuEhtVMq8jbUm8A5Hd355H4RlFCYfH2V8CQDCLwMOXgu14PAfARaiccgLu8coq
iAkcxD6itKNkgVZ2/a42Lo9Tk4iLMvuZyhEHmPpx+Vp18bzIp0UAYYPFI4sCQHHm
5c5c6ssQECVZT7T/qXJ2SiGug8kYiGa6P41C3GgX9ECsRdul92perJktYBa0I94z
nVdTpUlHr7ORCAg4ROECQEZBedRaihj18os80vQu8iUIyV5si8OEqxHYHYAhfqcv
ffO2krlgIZXYpuWafJL2jMr9AQafCg6JY5etb3411YU=
-----END RSA PRIVATE KEY-----""")
ASk = bytearray.fromhex("1a09d27bc2241afa82ea2458e6e8e6a3")

assetId = b"twelve"
spcData = sys.stdin.buffer.read()
spcContainer = SPCContainer.parse(spcData)

key = Utils.RSADecryptKey(spcContainer.wrappedKey, privateKey)
spcContainer.decrypt(key)

r2 = spcContainer.getTLLV(R2.TAG)

DASk = DFunction(r2.value, ASk)

sk_r1 = spcContainer.getTLLV(SK_R1.TAG)
sk_r1.decrypt(DASk)

sys.stderr.write(str(spcContainer))
sys.stderr.write("\n")


assetInfo = Utils.FetchContentKeyAndIV(assetId)
wrappedCK = Utils.AESEncryptDecrypt(assetInfo.contentKey, sk_r1.sk, None, SKDServerAESEncType.Encrypt, SKDServerAESEncMode.ECB)
ckcContainer = spcContainer.generateCKC(assetInfo, wrappedCK, sk_r1.r1)

ARk = spcContainer.generateARSeed()
ckcContainer.encrypt(ARk)

ckcData = ckcContainer.serialize()

sys.stderr.write(str(ckcContainer))
sys.stderr.write("\n")


sys.stdout.buffer.write(bytes(ckcData))
