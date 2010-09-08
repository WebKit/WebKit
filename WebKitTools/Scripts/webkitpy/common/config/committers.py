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

    def __init__(self, name, email_or_emails, irc_nickname=None):
        self.full_name = name
        if isinstance(email_or_emails, str):
            self.emails = [email_or_emails]
        else:
            self.emails = email_or_emails
        self.irc_nickname = irc_nickname
        self.can_review = False

    def bugzilla_email(self):
        # FIXME: We're assuming the first email is a valid bugzilla email,
        # which might not be right.
        return self.emails[0]

    def __str__(self):
        return '"%s" <%s>' % (self.full_name, self.emails[0])


class Reviewer(Committer):

    def __init__(self, name, email_or_emails, irc_nickname=None):
        Committer.__init__(self, name, email_or_emails, irc_nickname)
        self.can_review = True


# This is intended as a canonical, machine-readable list of all non-reviewer
# committers for WebKit.  If your name is missing here and you are a committer,
# please add it.  No review needed.  All reviewers are committers, so this list
# is only of committers who are not reviewers.


committers_unable_to_review = [
    Committer("Aaron Boodman", "aa@chromium.org", "aboodman"),
    Committer("Abhishek Arya", "inferno@chromium.org", "inferno-sec"),
    Committer("Adam Langley", "agl@chromium.org", "agl"),
    Committer("Albert J. Wong", "ajwong@chromium.org"),
    Committer("Alejandro G. Castro", ["alex@igalia.com", "alex@webkit.org"]),
    Committer("Alexander Kellett", ["lypanov@mac.com", "a-lists001@lypanov.net", "lypanov@kde.org"], "lypanov"),
    Committer("Alexander Pavlov", "apavlov@chromium.org"),
    Committer("Andre Boule", "aboule@apple.com"),
    Committer("Andrei Popescu", "andreip@google.com", "andreip"),
    Committer("Andrew Wellington", ["andrew@webkit.org", "proton@wiretapped.net"], "proton"),
    Committer("Andras Becsi", "abecsi@webkit.org", "bbandix"),
    Committer("Andy Estes", "aestes@apple.com", "estes"),
    Committer("Anthony Ricaud", "rik@webkit.org", "rik"),
    Committer("Anton Muhin", "antonm@chromium.org", "antonm"),
    Committer("Ben Murdoch", "benm@google.com", "benm"),
    Committer("Benjamin C Meyer", ["ben@meyerhome.net", "ben@webkit.org"], "icefox"),
    Committer("Benjamin Otte", ["otte@gnome.org", "otte@webkit.org"], "otte"),
    Committer("Benjamin Poulain", ["benjamin.poulain@nokia.com", "ikipou@gmail.com"]),
    Committer("Brent Fulgham", "bfulgham@webkit.org", "bfulgham"),
    Committer("Brett Wilson", "brettw@chromium.org", "brettx"),
    Committer("Brian Weinstein", "bweinstein@apple.com", "bweinstein"),
    Committer("Cameron McCormack", "cam@webkit.org", "heycam"),
    Committer("Carol Szabo", "carol.szabo@nokia.com"),
    Committer("Chang Shu", "Chang.Shu@nokia.com"),
    Committer("Chris Evans", "cevans@google.com"),
    Committer("Chris Petersen", "cpetersen@apple.com", "cpetersen"),
    Committer("Chris Rogers", "crogers@google.com", "crogers"),
    Committer("Christian Dywan", ["christian@twotoasts.de", "christian@webkit.org"]),
    Committer("Collin Jackson", "collinj@webkit.org"),
    Committer("Csaba Osztrogonac", "ossy@webkit.org", "ossy"),
    Committer("David Smith", ["catfish.man@gmail.com", "dsmith@webkit.org"], "catfishman"),
    Committer("Dean Jackson", "dino@apple.com", "dino"),
    Committer("Diego Gonzalez", ["diegohcg@webkit.org", "diego.gonzalez@openbossa.org"], "diegohcg"),
    Committer("Dirk Pranke", "dpranke@chromium.org"),
    Committer("Drew Wilson", "atwilson@chromium.org", "atwilson"),
    Committer("Eli Fidler", "eli@staikos.net", "QBin"),
    Committer("Enrica Casucci", "enrica@apple.com"),
    Committer("Erik Arvidsson", "arv@chromium.org", "arv"),
    Committer("Eric Roman", "eroman@chromium.org", "eroman"),
    Committer("Evan Martin", "evan@chromium.org", "evmar"),
    Committer("Evan Stade", "estade@chromium.org", "estade"),
    Committer("Feng Qian", "feng@chromium.org"),
    Committer("Fumitoshi Ukai", "ukai@chromium.org", "ukai"),
    Committer("Gabor Loki", "loki@webkit.org", "loki04"),
    Committer("Girish Ramakrishnan", ["girish@forwardbias.in", "ramakrishnan.girish@gmail.com"]),
    Committer("Graham Dennis", ["Graham.Dennis@gmail.com", "gdennis@webkit.org"]),
    Committer("Greg Bolsinga", "bolsinga@apple.com"),
    Committer("Hans Wennborg", "hans@chromium.org", "hwennborg"),
    Committer("Hin-Chung Lam", ["hclam@google.com", "hclam@chromium.org"]),
    Committer("Ilya Tikhonovsky", "loislo@chromium.org", "loislo"),
    Committer("Jakob Petsovits", ["jpetsovits@rim.com", "jpetso@gmx.at"], "jpetso"),
    Committer("Jakub Wieczorek", "jwieczorek@webkit.org", "fawek"),
    Committer("James Hawkins", ["jhawkins@chromium.org", "jhawkins@google.com"], "jhawkins"),
    Committer("Jay Civelli", "jcivelli@chromium.org", "jcivelli"),
    Committer("Jens Alfke", ["snej@chromium.org", "jens@apple.com"]),
    Committer("Jer Noble", "jer.noble@apple.com", "jernoble"),
    Committer("Jeremy Moskovich", ["playmobil@google.com", "jeremy@chromium.org"], "jeremymos"),
    Committer("Jessie Berlin", ["jberlin@webkit.org", "jberlin@apple.com"]),
    Committer("Jesus Sanchez-Palencia", ["jesus@webkit.org", "jesus.palencia@openbossa.org"], "jeez_"),
    Committer("Jocelyn Turcotte", "jocelyn.turcotte@nokia.com", "jturcotte"),
    Committer("Jochen Eisinger", "jochen@chromium.org", "jochen__"),
    Committer("John Abd-El-Malek", "jam@chromium.org", "jam"),
    Committer("John Gregg", ["johnnyg@google.com", "johnnyg@chromium.org"], "johnnyg"),
    Committer("Joost de Valk", ["joost@webkit.org", "webkit-dev@joostdevalk.nl"], "Altha"),
    Committer("Julie Parent", ["jparent@google.com", "jparent@chromium.org"], "jparent"),
    Committer("Julien Chaffraix", ["jchaffraix@webkit.org", "julien.chaffraix@gmail.com"]),
    Committer("Jungshik Shin", "jshin@chromium.org"),
    Committer("Justin Schuh", "jschuh@chromium.org", "jschuh"),
    Committer("Keishi Hattori", "keishi@webkit.org", "keishi"),
    Committer("Kelly Norton", "knorton@google.com"),
    Committer("Kent Hansen", "kent.hansen@nokia.com", "khansen"),
    Committer("Kinuko Yasuda", "kinuko@chromium.org", "kinuko"),
    Committer("Krzysztof Kowalczyk", "kkowalczyk@gmail.com"),
    Committer("Leandro Pereira", ["leandro@profusion.mobi", "leandro@webkit.org"], "acidx"),
    Committer("Levi Weintraub", "lweintraub@apple.com"),
    Committer("Lucas De Marchi", ["lucas.demarchi@profusion.mobi", "demarchi@webkit.org"], "demarchi"),
    Committer("Luiz Agostini", ["luiz@webkit.org", "luiz.agostini@openbossa.org"], "lca"),
    Committer("Mads Ager", "ager@chromium.org"),
    Committer("Marcus Voltis Bulach", "bulach@chromium.org"),
    Committer("Matt Lilek", ["webkit@mattlilek.com", "pewtermoose@webkit.org"]),
    Committer("Matt Perry", "mpcomplete@chromium.org"),
    Committer("Maxime Britto", ["maxime.britto@gmail.com", "britto@apple.com"]),
    Committer("Maxime Simon", ["simon.maxime@gmail.com", "maxime.simon@webkit.org"], "maxime.simon"),
    Committer("Michelangelo De Simone", "michelangelo@webkit.org", "michelangelo"),
    Committer("Mike Belshe", ["mbelshe@chromium.org", "mike@belshe.com"]),
    Committer("Mike Fenton", ["mifenton@rim.com", "mike.fenton@torchmobile.com"], "mfenton"),
    Committer("Mike Thole", ["mthole@mikethole.com", "mthole@apple.com"]),
    Committer("Mikhail Naganov", "mnaganov@chromium.org"),
    Committer("MORITA Hajime", "morrita@google.com", "morrita"),
    Committer("Nico Weber", ["thakis@chromium.org", "thakis@google.com"], "thakis"),
    Committer("Noam Rosenthal", "noam.rosenthal@nokia.com", "noamr"),
    Committer("Pam Greene", "pam@chromium.org", "pamg"),
    Committer("Peter Kasting", ["pkasting@google.com", "pkasting@chromium.org"], "pkasting"),
    Committer("Philippe Normand", ["pnormand@igalia.com", "philn@webkit.org"], "philn-tp"),
    Committer("Pierre d'Herbemont", ["pdherbemont@free.fr", "pdherbemont@apple.com"], "pdherbemont"),
    Committer("Pierre-Olivier Latour", "pol@apple.com", "pol"),
    Committer("Robert Hogan", ["robert@webkit.org", "robert@roberthogan.net", "lists@roberthogan.net"], "mwenge"),
    Committer("Roland Steiner", "rolandsteiner@chromium.org"),
    Committer("Ryosuke Niwa", "rniwa@webkit.org", "rniwa"),
    Committer("Satish Sampath", "satish@chromium.org"),
    Committer("Scott Violet", "sky@chromium.org", "sky"),
    Committer("Stephen White", "senorblanco@chromium.org", "senorblanco"),
    Committer("Tony Gentilcore", "tonyg@chromium.org", "tonyg-cr"),
    Committer("Trey Matteson", "trey@usa.net", "trey"),
    Committer("Tristan O'Tierney", ["tristan@otierney.net", "tristan@apple.com"]),
    Committer("Vangelis Kokkevis", "vangelis@chromium.org", "vangelis"),
    Committer("Victor Wang", "victorw@chromium.org", "victorw"),
    Committer("Vitaly Repeshko", "vitalyr@chromium.org"),
    Committer("William Siegrist", "wsiegrist@apple.com", "wms"),
    Committer("Xiaomei Ji", "xji@chromium.org", "xji"),
    Committer("Yael Aharon", "yael.aharon@nokia.com"),
    Committer("Yaar Schnitman", ["yaar@chromium.org", "yaar@google.com"]),
    Committer("Yong Li", ["yong.li.webkit@gmail.com", "yong.li@torchmobile.com"], "yong"),
    Committer("Yongjun Zhang", "yongjun.zhang@nokia.com"),
    Committer("Yuzo Fujishima", "yuzo@google.com", "yuzo"),
    Committer("Zhenyao Mo", "zmo@google.com", "zhenyao"),
    Committer("Zoltan Herczeg", "zherczeg@webkit.org", "zherczeg"),
    Committer("Zoltan Horvath", ["zoltan@webkit.org", "hzoltan@inf.u-szeged.hu", "horvath.zoltan.6@stud.u-szeged.hu"], "zoltan"),
]


# This is intended as a canonical, machine-readable list of all reviewers for
# WebKit.  If your name is missing here and you are a reviewer, please add it.
# No review needed.


reviewers_list = [
    Reviewer("Ada Chan", "adachan@apple.com", "chanada"),
    Reviewer("Adam Barth", "abarth@webkit.org", "abarth"),
    Reviewer("Adam Roben", "aroben@apple.com", "aroben"),
    Reviewer("Adam Treat", ["treat@kde.org", "treat@webkit.org", "atreat@rim.com"], "manyoso"),
    Reviewer("Adele Peterson", "adele@apple.com", "adele"),
    Reviewer("Alexey Proskuryakov", ["ap@webkit.org", "ap@apple.com"], "ap"),
    Reviewer("Alice Liu", "alice.liu@apple.com", "aliu"),
    Reviewer("Alp Toker", ["alp@nuanti.com", "alp@atoker.com", "alp@webkit.org"], "alp"),
    Reviewer("Anders Carlsson", ["andersca@apple.com", "acarlsson@apple.com"], "andersca"),
    Reviewer("Andreas Kling", "andreas.kling@nokia.com", "kling"),
    Reviewer("Antonio Gomes", "tonikitoo@webkit.org", "tonikitoo"),
    Reviewer("Antti Koivisto", ["koivisto@iki.fi", "antti@apple.com", "antti.j.koivisto@nokia.com"], "anttik"),
    Reviewer("Ariya Hidayat", ["ariya@sencha.com", "ariya.hidayat@gmail.com", "ariya@webkit.org"], "ariya"),
    Reviewer("Beth Dakin", "bdakin@apple.com", "dethbakin"),
    Reviewer("Brady Eidson", "beidson@apple.com", "bradee-oh"),
    Reviewer("Cameron Zwarich", ["zwarich@apple.com", "cwzwarich@apple.com", "cwzwarich@webkit.org"]),
    Reviewer("Chris Blumenberg", "cblu@apple.com", "cblu"),
    Reviewer("Chris Marrin", "cmarrin@apple.com", "cmarrin"),
    Reviewer("Chris Fleizach", "cfleizach@apple.com", "cfleizach"),
    Reviewer("Chris Jerdonek", "cjerdonek@webkit.org", "cjerdonek"),
    Reviewer("Dan Bernstein", ["mitz@webkit.org", "mitz@apple.com"], "mitzpettel"),
    Reviewer("Daniel Bates", "dbates@webkit.org", "dydz"),
    Reviewer("Darin Adler", "darin@apple.com", "darin"),
    Reviewer("Darin Fisher", ["fishd@chromium.org", "darin@chromium.org"], "fishd"),
    Reviewer("David Harrison", "harrison@apple.com", "harrison"),
    Reviewer("David Hyatt", "hyatt@apple.com", "hyatt"),
    Reviewer("David Kilzer", ["ddkilzer@webkit.org", "ddkilzer@apple.com"], "ddkilzer"),
    Reviewer("David Levin", "levin@chromium.org", "dave_levin"),
    Reviewer("Dimitri Glazkov", "dglazkov@chromium.org", "dglazkov"),
    Reviewer("Dirk Schulze", "krit@webkit.org", "krit"),
    Reviewer("Dmitry Titov", "dimich@chromium.org", "dimich"),
    Reviewer("Don Melton", "gramps@apple.com", "gramps"),
    Reviewer("Dumitru Daniliuc", "dumi@chromium.org", "dumi"),
    Reviewer("Eric Carlson", "eric.carlson@apple.com"),
    Reviewer("Eric Seidel", "eric@webkit.org", "eseidel"),
    Reviewer("Gavin Barraclough", "barraclough@apple.com", "gbarra"),
    Reviewer("Geoffrey Garen", "ggaren@apple.com", "ggaren"),
    Reviewer("George Staikos", ["staikos@kde.org", "staikos@webkit.org"]),
    Reviewer("Gustavo Noronha Silva", ["gns@gnome.org", "kov@webkit.org", "gustavo.noronha@collabora.co.uk"], "kov"),
    Reviewer("Holger Freyther", ["zecke@selfish.org", "zecke@webkit.org"], "zecke"),
    Reviewer("James Robinson", ["jamesr@chromium.org", "jamesr@google.com"], "jamesr"),
    Reviewer("Jan Alonzo", ["jmalonzo@gmail.com", "jmalonzo@webkit.org"], "janm"),
    Reviewer("Jeremy Orlow", "jorlow@chromium.org", "jorlow"),
    Reviewer("Jian Li", "jianli@chromium.org", "jianli"),
    Reviewer("John Sullivan", "sullivan@apple.com", "sullivan"),
    Reviewer("Jon Honeycutt", "jhoneycutt@apple.com", "jhoneycutt"),
    Reviewer("Joseph Pecoraro", ["joepeck@webkit.org", "pecoraro@apple.com"], "JoePeck"),
    Reviewer("Justin Garcia", "justin.garcia@apple.com", "justing"),
    Reviewer("Ken Kocienda", "kocienda@apple.com"),
    Reviewer("Kenneth Rohde Christiansen", ["kenneth@webkit.org", "kenneth.christiansen@openbossa.org", "kenneth.christiansen@gmail.com"], "kenne"),
    Reviewer("Kenneth Russell", "kbr@google.com", "kbr_google"),
    Reviewer("Kent Tamura", "tkent@chromium.org", "tkent"),
    Reviewer("Kevin Decker", "kdecker@apple.com", "superkevin"),
    Reviewer("Kevin McCullough", "kmccullough@apple.com", "maculloch"),
    Reviewer("Kevin Ollivier", ["kevino@theolliviers.com", "kevino@webkit.org"], "kollivier"),
    Reviewer("Lars Knoll", ["lars@trolltech.com", "lars@kde.org", "lars.knoll@nokia.com"], "lars"),
    Reviewer("Laszlo Gombos", "laszlo.1.gombos@nokia.com", "lgombos"),
    Reviewer("Maciej Stachowiak", "mjs@apple.com", "othermaciej"),
    Reviewer("Mark Rowe", "mrowe@apple.com", "bdash"),
    Reviewer("Martin Robinson", ["mrobinson@igalia.com", "mrobinson@webkit.org", "martin.james.robinson@gmail.com"], "mrobinson"),
    Reviewer("Nate Chapin", "japhet@chromium.org", "japhet"),
    Reviewer("Nikolas Zimmermann", ["zimmermann@kde.org", "zimmermann@physik.rwth-aachen.de", "zimmermann@webkit.org"], "wildfox"),
    Reviewer("Ojan Vafai", "ojan@chromium.org", "ojan"),
    Reviewer("Oliver Hunt", "oliver@apple.com", "olliej"),
    Reviewer("Pavel Feldman", "pfeldman@chromium.org", "pfeldman"),
    Reviewer("Richard Williamson", "rjw@apple.com", "rjw"),
    Reviewer("Rob Buis", ["rwlbuis@gmail.com", "rwlbuis@webkit.org"], "rwlbuis"),
    Reviewer("Sam Weinig", ["sam@webkit.org", "weinig@apple.com"], "weinig"),
    Reviewer("Shinichiro Hamaji", "hamaji@chromium.org", "hamaji"),
    Reviewer("Simon Fraser", "simon.fraser@apple.com", "smfr"),
    Reviewer("Simon Hausmann", ["hausmann@webkit.org", "hausmann@kde.org", "simon.hausmann@nokia.com"], "tronical"),
    Reviewer("Stephanie Lewis", "slewis@apple.com", "sundiamonde"),
    Reviewer("Steve Block", "steveblock@google.com", "steveblock"),
    Reviewer("Steve Falkenburg", "sfalken@apple.com", "sfalken"),
    Reviewer("Tim Omernick", "timo@apple.com"),
    Reviewer("Timothy Hatcher", ["timothy@apple.com", "timothy@hatcher.name"], "xenon"),
    Reviewer("Tony Chang", "tony@chromium.org", "tony^work"),
    Reviewer(u"Tor Arne Vestb\u00f8", ["vestbo@webkit.org", "tor.arne.vestbo@nokia.com"], "torarne"),
    Reviewer("Vicki Murley", "vicki@apple.com"),
    Reviewer("Xan Lopez", ["xan.lopez@gmail.com", "xan@gnome.org", "xan@webkit.org"], "xan"),
    Reviewer("Yury Semikhatsky", "yurys@chromium.org", "yurys"),
    Reviewer("Zack Rusin", "zack@kde.org", "zackr"),
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

    def committer_by_name(self, name):
        # This could be made into a hash lookup if callers need it to be fast.
        for committer in self.committers():
            if committer.full_name == name:
                return committer

    def committer_by_email(self, email):
        return self._email_to_committer_map().get(email)

    def reviewer_by_email(self, email):
        committer = self.committer_by_email(email)
        if committer and not committer.can_review:
            return None
        return committer
