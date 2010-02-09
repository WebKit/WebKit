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

    def __init__(self, name, email_or_emails):
        self.full_name = name
        if isinstance(email_or_emails, str):
            self.emails = [email_or_emails]
        else:
            self.emails = email_or_emails
        self.can_review = False

    def bugzilla_email(self):
        # FIXME: We're assuming the first email is a valid bugzilla email,
        # which might not be right.
        return self.emails[0]

    def __str__(self):
        return '"%s" <%s>' % (self.full_name, self.emails[0])


class Reviewer(Committer):

    def __init__(self, name, email_or_emails):
        Committer.__init__(self, name, email_or_emails)
        self.can_review = True


# This is intended as a canonical, machine-readable list of all non-reviewer
# committers for WebKit.  If your name is missing here and you are a committer,
# please add it.  No review needed.  All reviewers are committers, so this list
# is only of committers who are not reviewers.


committers_unable_to_review = [
    Committer("Aaron Boodman", "aa@chromium.org"),
    Committer("Adam Langley", "agl@chromium.org"),
    Committer("Albert J. Wong", "ajwong@chromium.org"),
    Committer("Alejandro G. Castro", ["alex@igalia.com", "alex@webkit.org"]),
    Committer("Alexander Kellett", ["lypanov@mac.com", "a-lists001@lypanov.net", "lypanov@kde.org"]),
    Committer("Alexander Pavlov", "apavlov@chromium.org"),
    Committer("Andre Boule", "aboule@apple.com"),
    Committer("Andrew Wellington", ["andrew@webkit.org", "proton@wiretapped.net"]),
    Committer("Andras Becsi", "abecsi@webkit.org"),
    Committer("Anthony Ricaud", "rik@webkit.org"),
    Committer("Anton Muhin", "antonm@chromium.org"),
    Committer("Antonio Gomes", "tonikitoo@webkit.org"),
    Committer("Ben Murdoch", "benm@google.com"),
    Committer("Benjamin C Meyer", ["ben@meyerhome.net", "ben@webkit.org"]),
    Committer("Benjamin Otte", ["otte@gnome.org", "otte@webkit.org"]),
    Committer("Brent Fulgham", "bfulgham@webkit.org"),
    Committer("Brett Wilson", "brettw@chromium.org"),
    Committer("Brian Weinstein", "bweinstein@apple.com"),
    Committer("Cameron McCormack", "cam@webkit.org"),
    Committer("Carol Szabo", "carol.szabo@nokia.com"),
    Committer("Chang Shu", "chang.shu@nokia.com"),
    Committer("Chris Fleizach", "cfleizach@apple.com"),
    Committer("Chris Jerdonek", "cjerdonek@webkit.org"),
    Committer("Chris Marrin", "cmarrin@apple.com"),
    Committer("Chris Petersen", "cpetersen@apple.com"),
    Committer("Christian Dywan", ["christian@twotoasts.de", "christian@webkit.org"]),
    Committer("Collin Jackson", "collinj@webkit.org"),
    Committer("Csaba Osztrogonac", "ossy@webkit.org"),
    Committer("Daniel Bates", "dbates@webkit.org"),
    Committer("David Smith", ["catfish.man@gmail.com", "dsmith@webkit.org"]),
    Committer("Dean Jackson", "dino@apple.com"),
    Committer("Dirk Pranke", "dpranke@chromium.org"),
    Committer("Drew Wilson", "atwilson@chromium.org"),
    Committer("Dumitru Daniliuc", "dumi@chromium.org"),
    Committer("Eli Fidler", "eli@staikos.net"),
    Committer("Enrica Casucci", "enrica@apple.com"),
    Committer("Erik Arvidsson", "arv@chromium.org"),
    Committer("Eric Roman", "eroman@chromium.org"),
    Committer("Feng Qian", "feng@chromium.org"),
    Committer("Fumitoshi Ukai", "ukai@chromium.org"),
    Committer("Gabor Loki", "loki@webkit.org"),
    Committer("Girish Ramakrishnan", ["girish@forwardbias.in", "ramakrishnan.girish@gmail.com"]),
    Committer("Graham Dennis", ["Graham.Dennis@gmail.com", "gdennis@webkit.org"]),
    Committer("Greg Bolsinga", "bolsinga@apple.com"),
    Committer("Hin-Chung Lam", ["hclam@google.com", "hclam@chromium.org"]),
    Committer("Jakob Petsovits", ["jpetsovits@rim.com", "jpetso@gmx.at"]),
    Committer("Jens Alfke", ["snej@chromium.org", "jens@apple.com"]),
    Committer("Jeremy Moskovich", ["playmobil@google.com", "jeremy@chromium.org"]),
    Committer("Jessie Berlin", ["jberlin@webkit.org", "jberlin@apple.com"]),
    Committer("Jian Li", "jianli@chromium.org"),
    Committer("John Abd-El-Malek", "jam@chromium.org"),
    Committer("Joost de Valk", ["joost@webkit.org", "webkit-dev@joostdevalk.nl"]),
    Committer("Joseph Pecoraro", "joepeck@webkit.org"),
    Committer("Julie Parent", ["jparent@google.com", "jparent@chromium.org"]),
    Committer("Julien Chaffraix", ["jchaffraix@webkit.org", "julien.chaffraix@gmail.com"]),
    Committer("Jungshik Shin", "jshin@chromium.org"),
    Committer("Keishi Hattori", "keishi@webkit.org"),
    Committer("Kelly Norton", "knorton@google.com"),
    Committer("Kenneth Russell", "kbr@google.com"),
    Committer("Kent Tamura", "tkent@chromium.org"),
    Committer("Krzysztof Kowalczyk", "kkowalczyk@gmail.com"),
    Committer("Levi Weintraub", "lweintraub@apple.com"),
    Committer("Mads Ager", "ager@chromium.org"),
    Committer("Matt Lilek", ["webkit@mattlilek.com", "pewtermoose@webkit.org"]),
    Committer("Matt Perry", "mpcomplete@chromium.org"),
    Committer("Maxime Britto", ["maxime.britto@gmail.com", "britto@apple.com"]),
    Committer("Maxime Simon", ["simon.maxime@gmail.com", "maxime.simon@webkit.org"]),
    Committer("Martin Robinson", ["mrobinson@webkit.org", "martin.james.robinson@gmail.com"]),
    Committer("Michelangelo De Simone", "michelangelo@webkit.org"),
    Committer("Mike Belshe", ["mbelshe@chromium.org", "mike@belshe.com"]),
    Committer("Mike Fenton", ["mike.fenton@torchmobile.com", "mifenton@rim.com"]),
    Committer("Mike Thole", ["mthole@mikethole.com", "mthole@apple.com"]),
    Committer("Mikhail Naganov", "mnaganov@chromium.org"),
    Committer("Ojan Vafai", "ojan@chromium.org"),
    Committer("Pam Greene", "pam@chromium.org"),
    Committer("Peter Kasting", ["pkasting@google.com", "pkasting@chromium.org"]),
    Committer("Philippe Normand", ["pnormand@igalia.com", "philn@webkit.org"]),
    Committer("Pierre d'Herbemont", ["pdherbemont@free.fr", "pdherbemont@apple.com"]),
    Committer("Pierre-Olivier Latour", "pol@apple.com"),
    Committer("Roland Steiner", "rolandsteiner@chromium.org"),
    Committer("Ryosuke Niwa", "rniwa@webkit.org"),
    Committer("Scott Violet", "sky@chromium.org"),
    Committer("Stephen White", "senorblanco@chromium.org"),
    Committer("Steve Block", "steveblock@google.com"),
    Committer("Tony Chang", "tony@chromium.org"),
    Committer("Trey Matteson", "trey@usa.net"),
    Committer("Tristan O'Tierney", ["tristan@otierney.net", "tristan@apple.com"]),
    Committer("Victor Wang", "victorw@chromium.org"),
    Committer("William Siegrist", "wsiegrist@apple.com"),
    Committer("Yael Aharon", "yael.aharon@nokia.com"),
    Committer("Yaar Schnitman", ["yaar@chromium.org", "yaar@google.com"]),
    Committer("Yong Li", ["yong.li@torchmobile.com", "yong.li.webkit@gmail.com"]),
    Committer("Yongjun Zhang", "yongjun.zhang@nokia.com"),
    Committer("Yury Semikhatsky", "yurys@chromium.org"),
    Committer("Yuzo Fujishima", "yuzo@google.com"),
    Committer("Zoltan Herczeg", "zherczeg@webkit.org"),
    Committer("Zoltan Horvath", "zoltan@webkit.org"),
]


# This is intended as a canonical, machine-readable list of all reviewers for
# WebKit.  If your name is missing here and you are a reviewer, please add it.
# No review needed.


reviewers_list = [
    Reviewer("Ada Chan", "adachan@apple.com"),
    Reviewer("Adam Barth", "abarth@webkit.org"),
    Reviewer("Adam Roben", "aroben@apple.com"),
    Reviewer("Adam Treat", ["treat@kde.org", "treat@webkit.org"]),
    Reviewer("Adele Peterson", "adele@apple.com"),
    Reviewer("Alexey Proskuryakov", ["ap@webkit.org", "ap@apple.com"]),
    Reviewer("Alice Liu", "alice.liu@apple.com"),
    Reviewer("Alp Toker", ["alp@nuanti.com", "alp@atoker.com", "alp@webkit.org"]),
    Reviewer("Anders Carlsson", ["andersca@apple.com", "acarlsson@apple.com"]),
    Reviewer("Antti Koivisto", ["koivisto@iki.fi", "antti@apple.com"]),
    Reviewer("Ariya Hidayat", ["ariya.hidayat@gmail.com", "ariya@webkit.org"]),
    Reviewer("Beth Dakin", "bdakin@apple.com"),
    Reviewer("Brady Eidson", "beidson@apple.com"),
    Reviewer("Cameron Zwarich", ["zwarich@apple.com", "cwzwarich@apple.com", "cwzwarich@webkit.org"]),
    Reviewer("Chris Blumenberg", "cblu@apple.com"),
    Reviewer("Dan Bernstein", ["mitz@webkit.org", "mitz@apple.com"]),
    Reviewer("Darin Adler", "darin@apple.com"),
    Reviewer("Darin Fisher", ["fishd@chromium.org", "darin@chromium.org"]),
    Reviewer("David Harrison", "harrison@apple.com"),
    Reviewer("David Hyatt", "hyatt@apple.com"),
    Reviewer("David Kilzer", ["ddkilzer@webkit.org", "ddkilzer@apple.com"]),
    Reviewer("David Levin", "levin@chromium.org"),
    Reviewer("Dimitri Glazkov", "dglazkov@chromium.org"),
    Reviewer("Dirk Schulze", "krit@webkit.org"),
    Reviewer("Dmitry Titov", "dimich@chromium.org"),
    Reviewer("Don Melton", "gramps@apple.com"),
    Reviewer("Eric Carlson", "eric.carlson@apple.com"),
    Reviewer("Eric Seidel", "eric@webkit.org"),
    Reviewer("Gavin Barraclough", "barraclough@apple.com"),
    Reviewer("Geoffrey Garen", "ggaren@apple.com"),
    Reviewer("George Staikos", ["staikos@kde.org", "staikos@webkit.org"]),
    Reviewer("Gustavo Noronha Silva", ["gns@gnome.org", "kov@webkit.org"]),
    Reviewer("Holger Freyther", ["zecke@selfish.org", "zecke@webkit.org"]),
    Reviewer("Jan Alonzo", ["jmalonzo@gmail.com", "jmalonzo@webkit.org"]),
    Reviewer("Jeremy Orlow", "jorlow@chromium.org"),
    Reviewer("John Sullivan", "sullivan@apple.com"),
    Reviewer("Jon Honeycutt", "jhoneycutt@apple.com"),
    Reviewer("Justin Garcia", "justin.garcia@apple.com"),
    Reviewer("Ken Kocienda", "kocienda@apple.com"),
    Reviewer("Kenneth Rohde Christiansen", ["kenneth@webkit.org", "kenneth.christiansen@openbossa.org"]),
    Reviewer("Kevin Decker", "kdecker@apple.com"),
    Reviewer("Kevin McCullough", "kmccullough@apple.com"),
    Reviewer("Kevin Ollivier", ["kevino@theolliviers.com", "kevino@webkit.org"]),
    Reviewer("Lars Knoll", ["lars@trolltech.com", "lars@kde.org"]),
    Reviewer("Laszlo Gombos", "laszlo.1.gombos@nokia.com"),
    Reviewer("Maciej Stachowiak", "mjs@apple.com"),
    Reviewer("Mark Rowe", "mrowe@apple.com"),
    Reviewer("Nate Chapin", "japhet@chromium.org"),
    Reviewer("Nikolas Zimmermann", ["zimmermann@kde.org", "zimmermann@physik.rwth-aachen.de", "zimmermann@webkit.org"]),
    Reviewer("Oliver Hunt", "oliver@apple.com"),
    Reviewer("Pavel Feldman", "pfeldman@chromium.org"),
    Reviewer("Richard Williamson", "rjw@apple.com"),
    Reviewer("Rob Buis", ["rwlbuis@gmail.com", "rwlbuis@webkit.org"]),
    Reviewer("Sam Weinig", ["sam@webkit.org", "weinig@apple.com"]),
    Reviewer("Shinichiro Hamaji", "hamaji@chromium.org"),
    Reviewer("Simon Fraser", "simon.fraser@apple.com"),
    Reviewer("Simon Hausmann", ["hausmann@webkit.org", "hausmann@kde.org", "simon.hausmann@nokia.com"]),
    Reviewer("Stephanie Lewis", "slewis@apple.com"),
    Reviewer("Steve Falkenburg", "sfalken@apple.com"),
    Reviewer("Tim Omernick", "timo@apple.com"),
    Reviewer("Timothy Hatcher", ["timothy@hatcher.name", "timothy@apple.com"]),
    Reviewer(u'Tor Arne Vestb\xf8', "vestbo@webkit.org"),
    Reviewer("Vicki Murley", "vicki@apple.com"),
    Reviewer("Xan Lopez", ["xan.lopez@gmail.com", "xan@gnome.org", "xan@webkit.org"]),
    Reviewer("Zack Rusin", "zack@kde.org"),
]


class CommitterList:

    # Committers and reviewers are passed in to allow easy testing

    def __init__(self,
                 committers=committers_unable_to_review,
                 reviewers=reviewers_list):
        self._committers = committers + reviewers
        self._reviewers = reviewers
        self._committers_by_email = {}

    def committers(self):
        return self._committers

    def reviewers(self):
        return self._reviewers

    def _email_to_committer_map(self):
        if not len(self._committers_by_email):
            for committer in self._committers:
                for email in committer.emails:
                    self._committers_by_email[email] = committer
        return self._committers_by_email

    def committer_by_email(self, email):
        return self._email_to_committer_map().get(email)

    def reviewer_by_email(self, email):
        committer = self.committer_by_email(email)
        if committer and not committer.can_review:
            return None
        return committer
