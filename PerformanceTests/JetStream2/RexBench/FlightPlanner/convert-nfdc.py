# Copyright (C) 2017 Apple Inc. All rights reserved.
# Copyright (C) 2016-2017 Michael Saboff. All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This parses the legacy FAA NFDC text files APT.txt, AWY.txt, NAV.txt
# and FIX.txt into a JavaScript format that he Flight Planner can use.
# These text files can be downloaded from the FAA at
# https://nfdc.faa.gov/xwiki/bin/view/NFDC/28+Day+NASR+Subscription
# This script generates two JavaScript object literal files, one for
# waypoints and the other for airways.  Both files create maps, where
# the key is the waypoint or airway identifier and the value is the
# corresponding data.
#
# Run this file in a directory where APT.txt, AWY.txt, NAV.txt and FIX.txt
# are downloaded.

import re

latLongRE = re.compile('\s*(\d+)-(\d{2})-(\d{2}.\d{3,8})([NS])\s*(\d+)-(\d{2})-(\d{2}.\d{3,8})([EW])')
statesAbbreviationsToExclude = ['AK', 'HI']
navaidsToInclude = ['VOR', 'NDB']
airwayFixTypes = frozenset(['CN', 'NDB/DME', 'NDB', 'MIL-REP-PT', 'REP-PT', 'RNAV', 'VOR', 'VOR/DME', 'VORTAC'])
airwayTypes = frozenset(['J', 'T', 'Q', 'V'])


class WaypointData:
    def __init__(self):
        self.waypoints = []
        self.allNames = set()

    def parseLatLongDMS(self, latLongString):
        match = latLongRE.match(latLongString)
        latitude = float(match.group(1)) + (float(match.group(2)) * 60 + float(match.group(3))) / 3600
        if match.group(4) == 'S':
            latitude = -latitude

        longitude = float(match.group(5)) + (float(match.group(6)) * 60 + float(match.group(7))) / 3600
        if match.group(8) == 'W':
            longitude = -longitude

        return (latitude, longitude)

    def parseAirportData(self, airportFile):
        file = open(airportFile, 'r')

        for line in file:
            if line[0:3] != 'APT':
                continue

            if line[48:50] in statesAbbreviationsToExclude:
                continue

            facilityType = line[14:27].rstrip().title()

            latLong = self.parseLatLongDMS(line[523:538] + line[550:565])

            description = line[133:183].rstrip().title() + ' ' + facilityType + ', ' + line[93:133].rstrip().title() + ', ' + line[48:50]
            description = description.replace('"', '\\"');

            name = line[1210:1217].rstrip()
            if not len(name):
                name = line[27:31].rstrip()

            if name in self.allNames:
                print('Duplicate airport waypoint name {0}'.format(name))
                continue

            self.allNames.add(name)
            self.waypoints.append((name, facilityType, description, latLong[0], latLong[1]))

        file.close()

    def parseNavAidData(self, navaidFile):
        file = open(navaidFile, 'r')

        self.ndbs = []
        for line in file:
            if line[0:4] != 'NAV1':
                continue

            if line[144:147] == 'AIN':
                continue

            if not line[8:11] in navaidsToInclude:
                continue

            facilityType = line[8:28].rstrip()

            latLong = self.parseLatLongDMS(line[371:385] + line[396:410])

            name = line[4:8].rstrip()
            description = line[42:72].rstrip().title() + ' ' + line[8:28].rstrip()

            # Since NDBs can have the same name as a VOR, we keep track of them separately
            # and add NDBs below if the NDB has a unique name.
            if line[8:11] == 'NDB':
                self.ndbs.append((name, facilityType, description, latLong[0], latLong[1]))
            else:
                self.allNames.add(name)
                self.waypoints.append((name, facilityType, description, latLong[0], latLong[1]))

        for ndb in self.ndbs:
            if ndb[0] not in self.allNames:
                self.allNames.add(ndb[0])
                self.waypoints.append(ndb)

        file.close()

    def parseFixData(self, fixFile):
        file = open(fixFile, 'r')

        for line in file:
            if line[0:4] != 'FIX1':
                continue

            if line[4] < 'A' or line[4] > 'Z':
                continue

            facilityType = 'Intersection'

            latLong = self.parseLatLongDMS(line[66:80] + line[80:94])

            name = line[4:34].rstrip()
            description = name + ' Intersection'

            if name in self.allNames:
                print('Duplicate fix name {0}'.format(name))
                continue

            self.allNames.add(name)
            self.waypoints.append((name, facilityType, description, latLong[0], latLong[1], ''))

        file.close()

    def outputWaypointFile(self, outputFilename):
        sortedWaypoints = sorted(self.waypoints, key=lambda waypoint: waypoint[0])

        outputFile = open(outputFilename, 'w+')

        outputFile.write('var _faaWaypoints = {\n');
        isFirst = True
        for waypoint in sortedWaypoints:
            if isFirst:
                isFirst = False
            else:
                outputFile.write(',\n');
            outputFile.write('    "{0}":{{ "name":"{0}", "type":"{1}", "description":"{2}", "latitude":{3}, "longitude":{4}}}'.format(waypoint[0], waypoint[1], waypoint[2], waypoint[3], waypoint[4]))
        outputFile.write('\n};\n');


class AirwayData:
    def __init__(self):
        self.airways = []

    def parseAirwayData(self, airwayFile):
        file = open(airwayFile, 'r')

        self.currentAirway = ''
        self.airwayPoints = []

        for line in file:
            airway = line[4:8].rstrip()
            if airway != self.currentAirway:
                if self.currentAirway != '':
                    self.airways.append((self.currentAirway, self.airwayPoints))
                self.currentAirway = ''
                self.airwayPoints = []

            if line[0:4] != 'AWY2':
                continue

            if line[4] not in airwayTypes:
                continue

            if line[9] != ' ':
                continue

            if line[45:64].rstrip() not in airwayFixTypes:
                continue

            if self.currentAirway == '':
                self.currentAirway = airway

            if line[64:79].rstrip() == 'FIX':
                self.airwayPoints.append(line[15:45].rstrip())
            else:
                self.airwayPoints.append(line[116:120].rstrip())

        if self.currentAirway != '':
            self.airways.append((self.currentAirway, self.airwayPoints))

        file.close()

    def outputAirwayFile(self, outputFilename):
        sortedAirways = sorted(self.airways, key=lambda airways: airways[0])

        outputFile = open(outputFilename, 'w+')

        outputFile.write('var _faaAirways = {\n');
        isFirst = True
        for airway in sortedAirways:
            if isFirst:
                isFirst = False
            else:
                outputFile.write(',\n');
            outputFile.write('    "{0}":{{ "name":"{0}", "fixes":['.format(airway[0]))
            isFirstFix = True
            for fix in airway[1]:
                if isFirstFix:
                    isFirstFix = False
                else:
                    outputFile.write(', ');

                outputFile.write('"{0}"'.format(fix))
            outputFile.write(']}')
        outputFile.write('\n};\n');


def main():
    waypoints = WaypointData()
    waypoints.parseAirportData('APT.txt')
    waypoints.parseNavAidData('NAV.txt')
    waypoints.parseFixData('FIX.txt')
    waypoints.outputWaypointFile('waypoints.js')
    airways = AirwayData()
    airways.parseAirwayData('AWY.txt')
    airways.outputAirwayFile('airways.js')


if __name__ == "__main__":
    main()
