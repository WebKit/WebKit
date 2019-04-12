#! /usr/bin/python

# Copyright (C) 2019 Apple Inc. All rights reserved.
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

import re
import sys

icStatRecord = re.compile(" +(\w+)\(([^,]+), ([^)]+)\)([^:]*): (\d+)")
getByIdPrefix = "OperationGetById"
putByIdPrefix = "OperationPutById"


class ICStats:
    def __init__(self):
        self.combinedRecords = {}
        self.slowGetById = {}
        self.slowPutById = {}
        self.totalSlowGetById = 0
        self.totalSlowPutById = 0

    def parse(self, file):
        for line in file:
            match = re.match(icStatRecord, line)
            if match:
                operation = match.group(1)
                base = match.group(2)
                property = match.group(3)
                location = match.group(4).strip()
                count = int(match.group(5))
                recordKey = (operation, base, property, location)

                if recordKey not in self.combinedRecords:
                    self.combinedRecords[recordKey] = count
                else:
                    self.combinedRecords[recordKey] += count

                if operation.startswith(getByIdPrefix):
                    self.totalSlowGetById += count

                    slowGetByIdKey = (base, property)
                    if slowGetByIdKey not in self.slowGetById:
                        self.slowGetById[slowGetByIdKey] = count
                    else:
                        self.slowGetById[slowGetByIdKey] += count

                elif operation.startswith(putByIdPrefix):
                    self.totalSlowPutById += count

                    slowPutByIdKey = (base, property)
                    if slowPutByIdKey not in self.slowPutById:
                        self.slowPutById[slowPutByIdKey] = count
                    else:
                        self.slowPutById[slowPutByIdKey] += count

    def dumpStats(self):
        print "Total Slow getById = {0:>13,d}".format(self.totalSlowGetById)
        print "Total Slow putById = {0:>13,d}".format(self.totalSlowPutById)

        print "Operation                            Base                  Property                              Location          Count  % tot"
        print "-----------------------------------  --------------------  ------------------------------------  ------------  ---------   slow"

        keys = sorted(self.combinedRecords.keys(), key=lambda t: self.combinedRecords[t], reverse=True)
        for key in keys:
            count = self.combinedRecords[key]
            operation = key[0]
            base = key[1]
            property = key[2]

            if operation.startswith(getByIdPrefix):
                slowPercent = "  {0:>4.1f}%".format(float(self.slowGetById[(base, property)] * 100) / float(self.totalSlowGetById))
            elif operation.startswith(putByIdPrefix):
                slowPercent = "  {0:>4.1f}%".format(float(self.slowPutById[(base, property)] * 100) / float(self.totalSlowPutById))
            else:
                slowPercent = ""

            if len(property) > 36:
                property = property[0:32] + "..."

            print "{0:35}  {1:20}  {2:36}  {3:12}  {4:>9d}{5}".format(operation[0:34], base, property, key[3], count, slowPercent)


def usage():
    print "Usage: {0} [ic-stats-file]".format(sys.argv[0])
    print "        Where <ic-stats-file> is the results of using the useICStats option."
    exit(1)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1] == "-h" or sys.argv[1].lower() == "--help" or sys.argv[1] == "-?":
            usage()
        try:
            file = open(sys.argv[1], "r")
        except IOError as e:
            print "Couldn't open {0}, {1}".format(sys.argv[1], e.strerror)
            usage()
        except:
            print "Unexpected error:", sys.exc_info()[0]
            usage()
    else:
        file = sys.stdin

    icStats = ICStats()
    icStats.parse(file)
    icStats.dumpStats()
