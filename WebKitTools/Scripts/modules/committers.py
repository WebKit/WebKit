# Copyright (c) 2009, Google Inc. All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# WebKit's Python module for committer and reviewer validation

class Committer:
    def __init__(self, name, email):
        self.full_name = name
        self.bugzilla_email = email
        self.can_review = False

    def __str__(self):
        return '"%s" <%s>' % (self.full_name, self.bugzilla_email)

class Reviewer(Committer):
    def __init__(self, name, email):
        Committer.__init__(self, name, email)
        self.can_review = True

# All reviewers are committers, so this list is only of committers
# who are not reviewers.
committers_unable_to_review = [
    Committer("Aaron Boodman", "aa@chromium.org"),
    Committer("Adam Langley", "agl@chromium.org"),
    Committer("Albert J. Wong", "ajwong@chromium.org"),
    Committer("Antonio Gomes", "tonikitoo@webkit.org"),
    Committer("Anthony Ricaud", "rik@webkit.org"),
    Committer("Ben Murdoch", "benm@google.com"),
    Committer("Brent Fulgham", "bfulgham@webkit.org"),
    Committer("Brian Weinstein", "bweinstein@apple.com"),
    Committer("Cameron McCormack", "cam@webkit.org"),
    Committer("Collin Jackson", "collinj@webkit.org"),
    Committer("Csaba Osztrogonac", "ossy@webkit.org"),
    Committer("Daniel Bates", "dbates@webkit.org"),
    Committer("Drew Wilson", "atwilson@chromium.org"),
    Committer("Dirk Schulze", "krit@webkit.org"),
    Committer("Dmitry Titov", "dimich@chromium.org"),
    Committer("Eli Fidler", "eli@staikos.net"),
    Committer("Eric Roman", "eroman@chromium.org"),
    Committer("Fumitoshi Ukai", "ukai@chromium.org"),
    Committer("Greg Bolsinga", "bolsinga@apple.com"),
    Committer("Jeremy Moskovich", "playmobil@google.com"),
    Committer("Jeremy Orlow", "jorlow@chromium.org"),
    Committer("Jian Li", "jianli@chromium.org"),
    Committer("John Abd-El-Malek", "jam@chromium.org"),
    Committer("Joseph Pecoraro", "joepeck@webkit.org"),
    Committer("Julie Parent", "jparent@google.com"),
    Committer("Kenneth Rohde Christiansen", "kenneth@webkit.org"),
    Committer("Laszlo Gombos", "laszlo.1.gombos@nokia.com"),
    Committer("Nate Chapin", "japhet@chromium.org"),
    Committer("Ojan Vafai", "ojan@chromium.org"),
    Committer("Pam Greene", "pam@chromium.org"),
    Committer("Peter Kasting", "pkasting@google.com"),
    Committer("Pierre d'Herbemont", "pdherbemont@free.fr"),
    Committer("Ryosuke Niwa", "rniwa@webkit.org"),
    Committer("Scott Violet", "sky@chromium.org"),
    Committer("Shinichiro Hamaji", "hamaji@chromium.org"),
    Committer("Tony Chang", "tony@chromium.org"),
    Committer("Yael Aharon", "yael.aharon@nokia.com"),
    Committer("Yong Li", "yong.li@torchmobile.com"),
    Committer("Zoltan Horvath", "zoltan@webkit.org"),
]

reviewers_list = [
    Reviewer("Adam Barth", "abarth@webkit.org"),
    Reviewer("Adam Roben", "aroben@apple.com"),
    Reviewer("Adam Treat", "treat@kde.org"),
    Reviewer("Adele Peterson", "adele@apple.com"),
    Reviewer("Alexey Proskuryakov", "ap@webkit.org"),
    Reviewer("Anders Carlsson", "andersca@apple.com"),
    Reviewer("Antti Koivisto", "koivisto@iki.fi"),
    Reviewer("Ariya Hidayat", "ariya.hidayat@trolltech.com"),
    Reviewer("Brady Eidson", "beidson@apple.com"),
    Reviewer("Dan Bernstein", "mitz@webkit.org"),
    Reviewer("Darin Adler", "darin@apple.com"),
    Reviewer("Darin Fisher", "fishd@chromium.org"),
    Reviewer("David Hyatt", "hyatt@apple.com"),
    Reviewer("David Kilzer", "ddkilzer@webkit.org"),
    Reviewer("David Levin", "levin@chromium.org"),
    Reviewer("Dimitri Glazkov", "dglazkov@chromium.org"),
    Reviewer("Eric Carlson", "eric.carlson@apple.com"),
    Reviewer("Eric Seidel", "eric@webkit.org"),
    Reviewer("Gavin Barraclough", "barraclough@apple.com"),
    Reviewer("Geoffrey Garen", "ggaren@apple.com"),
    Reviewer("George Staikos", "staikos@kde.org"),
    Reviewer("Gustavo Noronha", "gns@gnome.org"),
    Reviewer("Holger Freyther", "zecke@selfish.org"),
    Reviewer("Jan Alonzo", "jmalonzo@gmail.com"),
    Reviewer("John Sullivan", "sullivan@apple.com"),
    Reviewer("Justin Garcia", "justin.garcia@apple.com"),
    Reviewer("Kevin McCullough", "kmccullough@apple.com"),
    Reviewer("Kevin Ollivier", "kevino@theolliviers.com"),
    Reviewer("Maciej Stachowiak", "mjs@apple.com"),
    Reviewer("Mark Rowe", "mrowe@apple.com"),
    Reviewer("Nikolas Zimmermann", "zimmermann@kde.org"),
    Reviewer("Oliver Hunt", "oliver@apple.com"),
    Reviewer("Pavel Feldman", "pfeldman@chromium.org"),
    Reviewer("Sam Weinig", "sam@webkit.org"),
    Reviewer("Simon Fraser", "simon.fraser@apple.com"),
    Reviewer("Simon Hausmann", "hausmann@webkit.org"),
    Reviewer("Steve Falkenburg", "sfalken@apple.com"),
    Reviewer("Timothy Hatcher", "timothy@hatcher.name"),
    Reviewer(u'Tor Arne Vestb\xf8', "vestbo@webkit.org"),
    Reviewer("Xan Lopez", "xan.lopez@gmail.com"),
]


class CommitterList:
    # Committers and reviewers are passed in to allow easy testing
    def __init__(self, committers=committers_unable_to_review, reviewers=reviewers_list):
        self._committers = committers + reviewers
        self._reviewers = reviewers
        self._committers_by_email = {}

    def committers(self):
        return self._committers

    def _email_to_committer_map(self):
        if not len(self._committers_by_email):
            for committer in self._committers:
                self._committers_by_email[committer.bugzilla_email] = committer
        return self._committers_by_email

    def committer_by_bugzilla_email(self, bugzilla_email):
        return self._email_to_committer_map().get(bugzilla_email)

    def reviewer_by_bugzilla_email(self, bugzilla_email):
        committer = self.committer_by_bugzilla_email(bugzilla_email)
        if committer and not committer.can_review:
            return None
        return committer
