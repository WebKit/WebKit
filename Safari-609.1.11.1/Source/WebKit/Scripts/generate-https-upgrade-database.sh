#!/bin/sh
#
# Copyright (C) 2018 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

# This script requires that HTTPSUpgradeList.txt has the following format:
#   1. It must be a plain text file with domains delimited by new lines ("\n").
#   2. The file must not contain duplicate domains.
#   3. All domains must be lowercase.
#   4. All domains must be IDNA encoded.
#
# Usage:
# $ sh ./generate-https-upgrade-database.sh <path to input list> <path to output database>

set -e

INPUT_FILE_PATH="${1}"
OUTPUT_FILE_PATH="${2}"

DB_VERSION="1";
DB_SCHEMA="CREATE TABLE hosts (host TEXT PRIMARY KEY);"

if [[ ! -f "${INPUT_FILE_PATH}" ]]; then
    echo "Invalid input file" 1>&2;
    exit 1
fi

if [[ -f "${OUTPUT_FILE_PATH}" ]]; then
    rm "${OUTPUT_FILE_PATH}"
fi

sqlite3 "${OUTPUT_FILE_PATH}" "PRAGMA user_version=${DB_VERSION}" "${DB_SCHEMA}" ".import ${INPUT_FILE_PATH} hosts" ".exit"
