# Copyright (c) 2011, Apple Inc. All rights reserved.
# Copyright (c) 2009, 2011, 2012 Google Inc. All rights reserved.
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
# WebKit's Python module for committer and reviewer validation.

from webkitpy.common.editdistance import edit_distance

class Account(object):
    def __init__(self, name, email_or_emails, irc_nickname_or_nicknames=None):
        assert(name)
        assert(email_or_emails)
        self.full_name = name
        if isinstance(email_or_emails, str):
            self.emails = [email_or_emails]
        else:
            self.emails = email_or_emails
        self.emails = map(lambda email: email.lower(), self.emails)  # Emails are case-insensitive.
        if isinstance(irc_nickname_or_nicknames, str):
            self.irc_nicknames = [irc_nickname_or_nicknames]
        else:
            self.irc_nicknames = irc_nickname_or_nicknames
        self.can_commit = False
        self.can_review = False

    def bugzilla_email(self):
        # FIXME: We're assuming the first email is a valid bugzilla email,
        # which might not be right.
        return self.emails[0]

    def __str__(self):
        return '"%s" <%s>' % (self.full_name, self.emails[0])

    def contains_string(self, search_string):
        string = search_string.lower()
        if string in self.full_name.lower():
            return True
        if self.irc_nicknames:
            for nickname in self.irc_nicknames:
                if string in nickname.lower():
                    return True
        for email in self.emails:
            if string in email:
                return True
        return False


class Contributor(Account):
    def __init__(self, name, email_or_emails, irc_nickname=None):
        Account.__init__(self, name, email_or_emails, irc_nickname)
        self.is_contributor = True


class Committer(Contributor):
    def __init__(self, name, email_or_emails, irc_nickname=None):
        Contributor.__init__(self, name, email_or_emails, irc_nickname)
        self.can_commit = True


class Reviewer(Committer):
    def __init__(self, name, email_or_emails, irc_nickname=None):
        Committer.__init__(self, name, email_or_emails, irc_nickname)
        self.can_review = True


# This is a list of email addresses that have bugzilla accounts but are not
# used for contributing (such as mailing lists).


watchers_who_are_not_contributors = [
    Account("Chromium Compositor Bugs", ["cc-bugs@google.com"], ""),
    Account("David Levin", ["levin+threading@chromium.org"], ""),
    Account("David Levin", ["levin+watchlist@chromium.org"], ""),
]


# This is a list of people (or bots) who are neither committers nor reviewers, but get
# frequently CC'ed by others on Bugzilla bugs, so their names should be
# supported by autocomplete. No review needed to add to the list.


contributors_who_are_not_committers = [
    Contributor("Adam Kallai", "kallai.adam@stud.u-szeged.hu", 'kadam'),
    Contributor("Aharon Lanin", "aharon@google.com"),
    Contributor("Alan Stearns", "stearns@adobe.com", 'astearns'),
    Contributor("Alexandre Elias", "aelias@chromium.org"),
    Contributor("Alexey Marinichev", ["amarinichev@chromium.org", "amarinichev@google.com"], "amarinichev"),
    Contributor("Ami Fischman", ["fischman@chromium.org", "fischman@google.com"], "amifischman0"),
    Contributor("Andras Piroska", "pandras@inf.u-szeged.hu", "andris88"),
    Contributor("Anne van Kesteren", "annevankesteren+webkit@gmail.com", "annevk"),
    Contributor("Annie Sullivan", "sullivan@chromium.org", "annie"),
    Contributor("Aryeh Gregor", "ayg@aryeh.name", "AryehGregor"),
    Contributor("Balazs Ankes", "ankes.balazs@stud.u-szeged.hu", 'abalazs'),
    Contributor("Commit Queue", "commit-queue@webkit.org"),
    Contributor("Dana Jansens", "danakj@chromium.org", "danakj"),
    Contributor("Daniel Sievers", "sievers@chromium.org"),
    Contributor("David Barr", "davidbarr@chromium.org", "barrbrain"),
    Contributor("David Dorwin", "ddorwin@chromium.org", "ddorwin"),
    Contributor("David Reveman", "reveman@chromium.org", "reveman"),
    Contributor("Douglas Davidson", "ddavidso@apple.com"),
    Contributor("Edward O'Connor", "eoconnor@apple.com", "hober"),
    Contributor("Eric Penner", "epenner@chromium.org", "epenner"),
    Contributor("Felician Marton", ["felician@inf.u-szeged.hu", "marton.felician.zoltan@stud.u-szeged.hu"], "Felician"),
    Contributor("Finnur Thorarinsson", ["finnur@chromium.org", "finnur.webkit@gmail.com"], "finnur"),
    Contributor("Grace Kloba", "klobag@chromium.org", "klobag"),
    Contributor("Greg Simon", "gregsimon@chromium.org", "gregsimon"),
    Contributor("Gregg Tavares", ["gman@google.com", "gman@chromium.org"], "gman"),
    Contributor("Ian Hickson", "ian@hixie.ch", "hixie"),
    Contributor("Janos Badics", "dicska@gmail.hu", 'dicska'),
    Contributor("John Bates", ["jbates@google.com", "jbates@chromium.org"], "jbates"),
    Contributor("John Bauman", ["jbauman@chromium.org", "jbauman@google.com"], "jbauman"),
    Contributor("Kaustubh Atrawalkar", ["kaustubh@motorola.com"], "silverroots"),
    Contributor("Kulanthaivel Palanichamy", "kulanthaivel@codeaurora.org", "kvel"),
    Contributor("Nandor Huszka", "huszka.nandor@stud.u-szeged.hu", "hnandor"),
    Contributor("Oliver Varga", ["voliver@inf.u-szeged.hu", "Varga.Oliver@stud.u-szeged.hu"], "TwistO"),
    Contributor("Peter Gal", "galpeter@inf.u-szeged.hu", "elecro"),
    Contributor("Peter Linss", "peter.linss@hp.com", "plinss"),
    Contributor("Radar WebKit Bug Importer", "webkit-bug-importer@group.apple.com"),
    Contributor("Roland Takacs", "takacs.roland@stud.u-szeged.hu", "rtakacs"),
    Contributor("Szilard Ledan-Muntean", "muntean-ledan.szilard@stud.u-szeged.hu", "szledan"),
    Contributor("Tab Atkins", ["tabatkins@google.com", "jackalmage@gmail.com"], "tabatkins"),
    Contributor("Tamas Czene", ["tczene@inf.u-szeged.hu", "Czene.Tamas@stud.u-szeged.hu"], "tczene"),
    Contributor("WebKit Review Bot", "webkit.review.bot@gmail.com", "sheriff-bot"),
    Contributor("Wyatt Carss", ["wcarss@chromium.org", "wcarss@google.com"], "wcarss"),
    Contributor("Zoltan Arvai", "zarvai@inf.u-szeged.hu", "azbest_hu"),
    Contributor("Zsolt Feher", "feherzs@inf.u-szeged.hu", "Smith"),
]


# This is intended as a canonical, machine-readable list of all non-reviewer
# committers for WebKit.  If your name is missing here and you are a committer,
# please add it.  No review needed.  All reviewers are committers, so this list
# is only of committers who are not reviewers.


committers_unable_to_review = [
    Committer("Aaron Boodman", "aa@chromium.org", "aboodman"),
    Committer("Abhishek Arya", "inferno@chromium.org", "inferno-sec"),
    Committer("Adam Klein", "adamk@chromium.org", "aklein"),
    Committer("Adam Langley", "agl@chromium.org", "agl"),
    Committer("Ademar de Souza Reis Jr", ["ademar.reis@gmail.com", "ademar@webkit.org"], "ademar"),
    Committer("Adrienne Walker", ["enne@google.com", "enne@chromium.org"], "enne"),
    Committer("Albert J. Wong", "ajwong@chromium.org"),
    Committer("Alexander Kellett", ["lypanov@mac.com", "a-lists001@lypanov.net", "lypanov@kde.org"], "lypanov"),
    Committer("Alexander Pavlov", "apavlov@chromium.org", "apavlov"),
    Committer("Alexandru Chiculita", "achicu@adobe.com", "achicu"),
    Committer("Alexis Menard", ["alexis.menard@openbossa.org", "menard@kde.org", "alexis.menard@nokia.com"], "darktears"),
    Committer("Alice Boxhall", "aboxhall@chromium.org", "aboxhall"),
    Committer("Alok Priyadarshi", "alokp@chromium.org", "alokp"),
    Committer("Amruth Raj", "amruthraj@motorola.com", "amruthraj"),
    Committer("Andre Boule", "aboule@apple.com"),
    Committer("Andrei Popescu", "andreip@google.com", "andreip"),
    Committer("Andrew Wellington", ["andrew@webkit.org", "proton@wiretapped.net"], "proton"),
    Committer("Andrew Scherkus", "scherkus@chromium.org", "scherkus"),
    Committer("Andrey Kosyakov", "caseq@chromium.org", "caseq"),
    Committer("Andras Becsi", ["abecsi@webkit.org", "andras.becsi@nokia.com"], "bbandix"),
    Committer("Anna Cavender", "annacc@chromium.org", "annacc"),
    Committer("Anthony Ricaud", "rik@webkit.org", "rik"),
    Committer("Antoine Labour", "piman@chromium.org", "piman"),
    Committer("Anton D'Auria", "adauria@apple.com", "antonlefou"),
    Committer("Anton Muhin", "antonm@chromium.org", "antonm"),
    Committer("Balazs Kelemen", "kbalazs@webkit.org", "kbalazs"),
    Committer("Ben Murdoch", "benm@google.com", "benm"),
    Committer("Ben Wells", "benwells@chromium.org", "benwells"),
    Committer("Benjamin C Meyer", ["ben@meyerhome.net", "ben@webkit.org", "bmeyer@rim.com"], "icefox"),
    Committer("Benjamin Kalman", ["kalman@chromium.org", "kalman@google.com"], "kalman"),
    Committer("Benjamin Otte", ["otte@gnome.org", "otte@webkit.org"], "otte"),
    Committer("Brett Wilson", "brettw@chromium.org", "brettx"),
    Committer("Caio Marcelo de Oliveira Filho", ["cmarcelo@webkit.org", "caio.oliveira@openbossa.org"], "cmarcelo"),
    Committer("Cameron McCormack", ["cam@mcc.id.au", "cam@webkit.org"], "heycam"),
    Committer("Carlos Garcia Campos", ["cgarcia@igalia.com", "carlosgc@gnome.org", "carlosgc@webkit.org"], "KaL"),
    Committer("Carol Szabo", ["carol@webkit.org", "carol.szabo@nokia.com"], "cszabo1"),
    Committer("Cary Clark", ["caryclark@google.com", "caryclark@chromium.org"], "caryclark"),
    Committer("Chris Evans", ["cevans@google.com", "cevans@chromium.org"]),
    Committer("Chris Guillory", ["ctguil@chromium.org", "chris.guillory@google.com"], "ctguil"),
    Committer("Chris Petersen", "cpetersen@apple.com", "cpetersen"),
    Committer("Christian Dywan", ["christian@twotoasts.de", "christian@webkit.org", "christian@lanedo.com"]),
    Committer("Collin Jackson", "collinj@webkit.org", "collinjackson"),
    Committer("Cris Neckar", "cdn@chromium.org", "cneckar"),
    Committer("Dan Winship", "danw@gnome.org", "danw"),
    Committer("Daniel Cheng", "dcheng@chromium.org", "dcheng"),
    Committer("David Grogan", ["dgrogan@chromium.org", "dgrogan@google.com"], "dgrogan"),
    Committer("David Smith", ["catfish.man@gmail.com", "dsmith@webkit.org"], "catfishman"),
    Committer("Diego Gonzalez", ["diegohcg@webkit.org", "diego.gonzalez@openbossa.org"], "diegohcg"),
    Committer("Dmitry Lomov", ["dslomov@google.com", "dslomov@chromium.org"], "dslomov"),
    Committer("Dominic Cooney", ["dominicc@chromium.org", "dominicc@google.com"], "dominicc"),
    Committer("Dominic Mazzoni", ["dmazzoni@google.com", "dmazzoni@chromium.org"], "dmazzoni"),
    Committer("Drew Wilson", "atwilson@chromium.org", "atwilson"),
    Committer("Eli Fidler", ["eli@staikos.net", "efidler@rim.com"], "efidler"),
    Committer("Emil A Eklund", "eae@chromium.org", "eae"),
    Committer("Erik Arvidsson", "arv@chromium.org", "arv"),
    Committer("Eric Roman", "eroman@chromium.org", "eroman"),
    Committer("Eric Uhrhane", "ericu@chromium.org", "ericu"),
    Committer("Evan Martin", "evan@chromium.org", "evmar"),
    Committer("Evan Stade", "estade@chromium.org", "estade"),
    Committer("Fady Samuel", "fsamuel@chromium.org", "fsamuel"),
    Committer("Feng Qian", "feng@chromium.org"),
    Committer("Fumitoshi Ukai", "ukai@chromium.org", "ukai"),
    Committer("Gabor Loki", "loki@webkit.org", "loki04"),
    Committer("Gabor Rapcsanyi", ["rgabor@webkit.org", "rgabor@inf.u-szeged.hu"], "rgabor"),
    Committer("Gavin Peters", ["gavinp@chromium.org", "gavinp@webkit.org", "gavinp@google.com"], "gavinp"),
    Committer("Girish Ramakrishnan", ["girish@forwardbias.in", "ramakrishnan.girish@gmail.com"], "girishr"),
    Committer("Graham Dennis", ["Graham.Dennis@gmail.com", "gdennis@webkit.org"]),
    Committer("Greg Bolsinga", "bolsinga@apple.com"),
    Committer("Gyuyoung Kim", ["gyuyoung.kim@samsung.com", "gyuyoung.kim@webkit.org"], "gyuyoung"),
    Committer("Hans Wennborg", "hans@chromium.org", "hwennborg"),
    Committer("Hayato Ito", "hayato@chromium.org", "hayato"),
    Committer("Helder Correia", "helder@sencha.com", "helder"),
    Committer("Hin-Chung Lam", ["hclam@google.com", "hclam@chromium.org"]),
    Committer("Igor Trindade Oliveira", ["igor.oliveira@webkit.org", "igor.oliveira@openbossa.org"], "igoroliveira"),
    Committer("Ilya Sherman", "isherman@chromium.org", "isherman"),
    Committer("Ilya Tikhonovsky", "loislo@chromium.org", "loislo"),
    Committer("Ivan Krsti\u0107", "ike@apple.com"),
    Committer("Jakob Petsovits", ["jpetsovits@rim.com", "jpetso@gmx.at"], "jpetso"),
    Committer("Jakub Wieczorek", "jwieczorek@webkit.org", "fawek"),
    Committer("James Hawkins", ["jhawkins@chromium.org", "jhawkins@google.com"], "jhawkins"),
    Committer("James Kozianski", ["koz@chromium.org", "koz@google.com"], "koz"),
    Committer("James Simonsen", "simonjam@chromium.org", "simonjam"),
    Committer("Jarred Nicholls", ["jarred@webkit.org", "jarred@sencha.com"], "jarrednicholls"),
    Committer("Jay Civelli", "jcivelli@chromium.org", "jcivelli"),
    Committer("Jeff Miller", "jeffm@apple.com", "jeffm7"),
    Committer("Jeffrey Pfau", ["jeffrey@endrift.com", "jpfau@apple.com"], "jpfau"),
    Committer("Jenn Braithwaite", "jennb@chromium.org", "jennb"),
    Committer("Jens Alfke", ["snej@chromium.org", "jens@apple.com"]),
    Committer("Jer Noble", "jer.noble@apple.com", "jernoble"),
    Committer("Jeremy Moskovich", ["playmobil@google.com", "jeremy@chromium.org"], "jeremymos"),
    Committer("Jessie Berlin", ["jberlin@webkit.org", "jberlin@apple.com"]),
    Committer("Jesus Sanchez-Palencia", ["jesus@webkit.org", "jesus.palencia@openbossa.org"], "jeez_"),
    Committer("Jia Pu", "jpu@apple.com"),
    Committer("Jocelyn Turcotte", "jocelyn.turcotte@nokia.com", "jturcotte"),
    Committer("Jochen Eisinger", "jochen@chromium.org", "jochen__"),
    Committer("John Abd-El-Malek", "jam@chromium.org", "jam"),
    Committer("John Gregg", ["johnnyg@google.com", "johnnyg@chromium.org"], "johnnyg"),
    Committer("John Knottenbelt", "jknotten@chromium.org", "jknotten"),
    Committer("Johnny Ding", ["jnd@chromium.org", "johnnyding.webkit@gmail.com"], "johnnyding"),
    Committer("Jon Lee", "jonlee@apple.com", "jonlee"),
    Committer("Joone Hur", ["joone.hur@collabora.co.uk", "joone@kldp.org", "joone@webkit.org"], "joone"),
    Committer("Joost de Valk", ["joost@webkit.org", "webkit-dev@joostdevalk.nl"], "Altha"),
    Committer("Joshua Bell", ["jsbell@chromium.org", "jsbell@google.com"], "jsbell"),
    Committer("Julie Parent", ["jparent@google.com", "jparent@chromium.org"], "jparent"),
    Committer("Jungshik Shin", "jshin@chromium.org"),
    Committer("Justin Schuh", "jschuh@chromium.org", "jschuh"),
    Committer("Keishi Hattori", "keishi@webkit.org", "keishi"),
    Committer("Kelly Norton", "knorton@google.com"),
    Committer("Kenichi Ishibashi", "bashi@chromium.org", "bashi"),
    Committer("Kenji Imasaki", "imasaki@chromium.org", "imasaki"),
    Committer("Kent Hansen", "kent.hansen@nokia.com", "khansen"),
    Committer(u"Kim Gr\u00f6nholm", "kim.1.gronholm@nokia.com"),
    Committer("Kimmo Kinnunen", ["kimmo.t.kinnunen@nokia.com", "kimmok@iki.fi", "ktkinnun@webkit.org"], "kimmok"),
    Committer("Kinuko Yasuda", "kinuko@chromium.org", "kinuko"),
    Committer("Kristof Kosztyo", "kkristof@inf.u-szeged.hu", "kkristof"),
    Committer("Krzysztof Kowalczyk", "kkowalczyk@gmail.com"),
    Committer("Kwang Yul Seo", ["kwangyul.seo@gmail.com", "skyul@company100.net", "kseo@webkit.org"], "kwangseo"),
    Committer("Leandro Gracia Gil", "leandrogracia@chromium.org", "leandrogracia"),
    Committer("Leandro Pereira", ["leandro@profusion.mobi", "leandro@webkit.org"], "acidx"),
    Committer("Leo Yang", ["leo.yang@torchmobile.com.cn", "leoyang@webkit.org", "leoyang.webkit@gmail.com", "leo.yang.c@gmail.com"], "leoyang"),
    Committer("Levi Weintraub", ["leviw@chromium.org", "leviw@google.com", "lweintraub@apple.com"], "leviw"),
    Committer("Lucas De Marchi", ["demarchi@webkit.org", "lucas.demarchi@profusion.mobi"], "demarchi"),
    Committer("Lucas Forschler", ["lforschler@apple.com"], "lforschler"),
    Committer("Luke Macpherson", ["macpherson@chromium.org", "macpherson@google.com"], "macpherson"),
    Committer("Mads Ager", "ager@chromium.org"),
    Committer("Mahesh Kulkarni", "mahesh.kulkarni@nokia.com", "maheshk"),
    Committer("Marcus Voltis Bulach", "bulach@chromium.org"),
    Committer("Mario Sanchez Prada", ["msanchez@igalia.com", "mario@webkit.org"], "msanchez"),
    Committer("Mark Hahnenberg", "mhahnenberg@apple.com"),
    Committer("Matt Delaney", "mdelaney@apple.com"),
    Committer("Matt Lilek", ["mlilek@apple.com", "webkit@mattlilek.com", "pewtermoose@webkit.org"], "pewtermoose"),
    Committer("Matt Perry", "mpcomplete@chromium.org"),
    Committer("Maxime Britto", ["maxime.britto@gmail.com", "britto@apple.com"]),
    Committer("Maxime Simon", ["simon.maxime@gmail.com", "maxime.simon@webkit.org"], "maxime.simon"),
    Committer("Michael Nordman", "michaeln@google.com", "michaeln"),
    Committer("Michelangelo De Simone", "michelangelo@webkit.org", "michelangelo"),
    Committer("Mihnea Ovidenie", "mihnea@adobe.com", "mihnea"),
    Committer("Mike Belshe", ["mbelshe@chromium.org", "mike@belshe.com"]),
    Committer("Mike Fenton", ["mifenton@rim.com", "mike.fenton@torchmobile.com"], "mfenton"),
    Committer("Mike Lawther", "mikelawther@chromium.org", "mikelawther"),
    Committer("Mike Reed", "reed@google.com", "reed"),
    Committer("Mike Thole", ["mthole@mikethole.com", "mthole@apple.com"]),
    Committer("Mikhail Naganov", "mnaganov@chromium.org"),
    Committer("Naoki Takano", ["honten@chromium.org", "takano.naoki@gmail.com"], "honten"),
    Committer("Nat Duca", ["nduca@chromium.org", "nduca@google.com"], "nduca"),
    Committer("Nayan Kumar K", ["nayankk@motorola.com", "nayankk@gmail.com"], "xc0ffee"),
    Committer("Nico Weber", ["thakis@chromium.org", "thakis@google.com"], "thakis"),
    Committer("Noel Gordon", ["noel.gordon@gmail.com", "noel@chromium.org", "noel@google.com"], "noel"),
    Committer("Pam Greene", "pam@chromium.org", "pamg"),
    Committer("Patrick Gansterer", ["paroga@paroga.com", "paroga@webkit.org"], "paroga"),
    Committer("Pavel Podivilov", "podivilov@chromium.org", "podivilov"),
    Committer("Peter Beverloo", ["peter@chromium.org", "beverloo@google.com"], "beverloo"),
    Committer("Peter Kasting", ["pkasting@google.com", "pkasting@chromium.org"], "pkasting"),
    Committer("Peter Varga", ["pvarga@webkit.org", "pvarga@inf.u-szeged.hu"], "stampho"),
    Committer("Pierre d'Herbemont", ["pdherbemont@free.fr", "pdherbemont@apple.com"], "pdherbemont"),
    Committer("Pierre-Olivier Latour", "pol@apple.com", "pol"),
    Committer("Pierre Rossi", "pierre.rossi@gmail.com", "elproxy"),
    Committer("Pratik Solanki", "psolanki@apple.com", "psolanki"),
    Committer("Qi Zhang", ["qi.2.zhang@nokia.com", "qi.zhang02180@gmail.com"], "qi"),
    Committer("Rafael Antognolli", "antognolli@profusion.mobi", "antognolli"),
    Committer("Raphael Kubo da Costa", ["kubo@profusion.mobi", "rakuco@FreeBSD.org"], "rakuco"),
    Committer("Ravi Kasibhatla", "ravi.kasibhatla@motorola.com", "kphanee"),
    Committer("Renata Hodovan", "reni@webkit.org", "reni"),
    Committer("Robert Hogan", ["robert@webkit.org", "robert@roberthogan.net", "lists@roberthogan.net"], "mwenge"),
    Committer("Roland Steiner", "rolandsteiner@chromium.org"),
    Committer("Ryuan Choi", "ryuan.choi@samsung.com", "ryuan"),
    Committer("Satish Sampath", "satish@chromium.org"),
    Committer("Scott Violet", "sky@chromium.org", "sky"),
    Committer("Sergio Villar Senin", ["svillar@igalia.com", "sergio@webkit.org"], "svillar"),
    Committer("Shawn Singh", "shawnsingh@chromium.org", "shawnsingh"),
    Committer("Siddharth Mathur", "siddharth.mathur@nokia.com", "simathur"),
    Committer("Steve Lacey", "sjl@chromium.org", "stevela"),
    Committer("Takashi Toyoshima", "toyoshim@chromium.org", "toyoshim"),
    Committer("Tim Horton", "timothy_horton@apple.com", "thorton"),
    Committer("Tom Zakrajsek", "tomz@codeaurora.org", "tomz"),
    Committer("Tommy Widenflycht", "tommyw@google.com", "tommyw"),
    Committer("Trey Matteson", "trey@usa.net", "trey"),
    Committer("Tristan O'Tierney", ["tristan@otierney.net", "tristan@apple.com"]),
    Committer("Vangelis Kokkevis", "vangelis@chromium.org", "vangelis"),
    Committer("Viatcheslav Ostapenko", "ostapenko.viatcheslav@nokia.com", "ostap"),
    Committer("Victor Wang", "victorw@chromium.org", "victorw"),
    Committer("Victoria Kirst", ["vrk@chromium.org", "vrk@google.com"], "vrk"),
    Committer("Vincent Scheib", "scheib@chromium.org", "scheib"),
    Committer("Vitaly Repeshko", "vitalyr@chromium.org"),
    Committer("William Siegrist", "wsiegrist@apple.com", "wms"),
    Committer("W. James MacLean", "wjmaclean@chromium.org", "wjmaclean"),
    Committer("Xianzhu Wang", ["wangxianzhu@chromium.org", "phnixwxz@gmail.com", "wangxianzhu@google.com"], "wangxianzhu"),
    Committer("Xiaomei Ji", "xji@chromium.org", "xji"),
    Committer("Yael Aharon", "yael.aharon@nokia.com", "yael"),
    Committer("Yaar Schnitman", ["yaar@chromium.org", "yaar@google.com"]),
    Committer("Yong Li", ["yong.li.webkit@gmail.com", "yong.li@torchmobile.com", "yoli@rim.com"], "yong"),
    Committer("Yongjun Zhang", ["yongjun.zhang@nokia.com", "yongjun_zhang@apple.com"]),
    Committer("Yuqiang Xian", "yuqiang.xian@intel.com"),
    Committer("Yi Shen", ["yi.4.shen@nokia.com", "shenyi2006@gmail.com"]),
    Committer("Yuta Kitamura", "yutak@chromium.org", "yutak"),
    Committer("Yuzo Fujishima", "yuzo@google.com", "yuzo"),
    Committer("Zeno Albisser", ["zeno@webkit.org", "zeno.albisser@nokia.com"], "zalbisser"),
    Committer("Zhenyao Mo", "zmo@google.com", "zhenyao"),
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
    Reviewer("Alejandro G. Castro", ["alex@igalia.com", "alex@webkit.org"], "alexg__"),
    Reviewer("Adele Peterson", "adele@apple.com", "adele"),
    Reviewer("Alexey Proskuryakov", ["ap@webkit.org", "ap@apple.com"], "ap"),
    Reviewer("Alice Liu", "alice.liu@apple.com", "aliu"),
    Reviewer("Alp Toker", ["alp@nuanti.com", "alp@atoker.com", "alp@webkit.org"], "alp"),
    Reviewer("Anders Carlsson", ["andersca@apple.com", "acarlsson@apple.com"], "andersca"),
    Reviewer("Andreas Kling", ["kling@webkit.org", "awesomekling@apple.com", "andreas.kling@nokia.com"], "kling"),
    Reviewer("Andy Estes", "aestes@apple.com", "estes"),
    Reviewer("Antonio Gomes", ["tonikitoo@webkit.org", "agomes@rim.com"], "tonikitoo"),
    Reviewer("Antti Koivisto", ["koivisto@iki.fi", "antti@apple.com", "antti.j.koivisto@nokia.com"], "anttik"),
    Reviewer("Ariya Hidayat", ["ariya.hidayat@gmail.com", "ariya@sencha.com", "ariya@webkit.org"], "ariya"),
    Reviewer("Benjamin Poulain", ["benjamin@webkit.org", "benjamin.poulain@nokia.com", "ikipou@gmail.com"], "benjaminp"),
    Reviewer("Beth Dakin", "bdakin@apple.com", "dethbakin"),
    Reviewer("Brady Eidson", "beidson@apple.com", "bradee-oh"),
    Reviewer("Brent Fulgham", "bfulgham@webkit.org", "bfulgham"),
    Reviewer("Brian Weinstein", "bweinstein@apple.com", "bweinstein"),
    Reviewer("Cameron Zwarich", ["zwarich@apple.com", "cwzwarich@apple.com", "cwzwarich@webkit.org"]),
    Reviewer("Chang Shu", ["cshu@webkit.org", "Chang.Shu@nokia.com"], "cshu"),
    Reviewer("Chris Blumenberg", "cblu@apple.com", "cblu"),
    Reviewer("Chris Marrin", "cmarrin@apple.com", "cmarrin"),
    Reviewer("Chris Fleizach", "cfleizach@apple.com", "cfleizach"),
    Reviewer("Chris Jerdonek", "cjerdonek@webkit.org", "cjerdonek"),
    Reviewer("Chris Rogers", "crogers@google.com", "crogers"),
    Reviewer(u"Csaba Osztrogon\u00e1c", "ossy@webkit.org", "ossy"),
    Reviewer("Dan Bernstein", ["mitz@webkit.org", "mitz@apple.com"], "mitzpettel"),
    Reviewer("Daniel Bates", ["dbates@webkit.org", "dbates@rim.com"], "dydz"),
    Reviewer("Darin Adler", "darin@apple.com", "darin"),
    Reviewer("Darin Fisher", ["fishd@chromium.org", "darin@chromium.org"], "fishd"),
    Reviewer("David Harrison", "harrison@apple.com", "harrison"),
    Reviewer("David Hyatt", "hyatt@apple.com", ["dhyatt", "hyatt"]),
    Reviewer("David Kilzer", ["ddkilzer@webkit.org", "ddkilzer@apple.com"], "ddkilzer"),
    Reviewer("David Levin", "levin@chromium.org", "dave_levin"),
    Reviewer("Dean Jackson", "dino@apple.com", "dino"),
    Reviewer("Dimitri Glazkov", "dglazkov@chromium.org", "dglazkov"),
    Reviewer("Dirk Pranke", "dpranke@chromium.org", "dpranke"),
    Reviewer("Dirk Schulze", "krit@webkit.org", "krit"),
    Reviewer("Dmitry Titov", "dimich@chromium.org", "dimich"),
    Reviewer("Don Melton", "gramps@apple.com", "gramps"),
    Reviewer("Dumitru Daniliuc", "dumi@chromium.org", "dumi"),
    Reviewer("Enrica Casucci", "enrica@apple.com", "enrica"),
    Reviewer("Eric Carlson", "eric.carlson@apple.com", "eric_carlson"),
    Reviewer("Eric Seidel", "eric@webkit.org", "eseidel"),
    Reviewer("Filip Pizlo", "fpizlo@apple.com", "pizlo"),
    Reviewer("Gavin Barraclough", "barraclough@apple.com", "gbarra"),
    Reviewer("Geoffrey Garen", "ggaren@apple.com", "ggaren"),
    Reviewer("George Staikos", ["staikos@kde.org", "staikos@webkit.org"]),
    Reviewer("Gustavo Noronha Silva", ["gns@gnome.org", "kov@webkit.org", "gustavo.noronha@collabora.co.uk", "gustavo.noronha@collabora.com"], "kov"),
    Reviewer("Hajime Morita", ["morrita@google.com", "morrita@chromium.org"], "morrita"),
    Reviewer("Holger Freyther", ["zecke@selfish.org", "zecke@webkit.org"], "zecke"),
    Reviewer("James Robinson", ["jamesr@chromium.org", "jamesr@google.com"], "jamesr"),
    Reviewer("Jan Alonzo", ["jmalonzo@gmail.com", "jmalonzo@webkit.org"], "janm"),
    Reviewer("Jeremy Orlow", ["jorlow@webkit.org", "jorlow@chromium.org"], "jorlow"),
    Reviewer("Jian Li", "jianli@chromium.org", "jianli"),
    Reviewer("John Sullivan", "sullivan@apple.com", "sullivan"),
    Reviewer("Jon Honeycutt", "jhoneycutt@apple.com", "jhoneycutt"),
    Reviewer("Joseph Pecoraro", ["joepeck@webkit.org", "pecoraro@apple.com"], "JoePeck"),
    Reviewer("Julien Chaffraix", ["jchaffraix@webkit.org", "julien.chaffraix@gmail.com", "jchaffraix@google.com", "jchaffraix@codeaurora.org"], "jchaffraix"),
    Reviewer("Justin Garcia", "justin.garcia@apple.com", "justing"),
    Reviewer("Ken Kocienda", "kocienda@apple.com"),
    Reviewer("Kenneth Rohde Christiansen", ["kenneth@webkit.org", "kenneth.christiansen@openbossa.org", "kenneth.christiansen@gmail.com"], ["kenne", "kenneth"]),
    Reviewer("Kenneth Russell", "kbr@google.com", "kbr_google"),
    Reviewer("Kent Tamura", ["tkent@chromium.org", "tkent@google.com"], "tkent"),
    Reviewer("Kentaro Hara", ["haraken@chromium.org"], "haraken"),
    Reviewer("Kevin Decker", "kdecker@apple.com", "superkevin"),
    Reviewer("Kevin McCullough", "kmccullough@apple.com", "maculloch"),
    Reviewer("Kevin Ollivier", ["kevino@theolliviers.com", "kevino@webkit.org"], "kollivier"),
    Reviewer("Lars Knoll", ["lars@trolltech.com", "lars@kde.org", "lars.knoll@nokia.com"], "lars"),
    Reviewer("Laszlo Gombos", "laszlo.1.gombos@nokia.com", "lgombos"),
    Reviewer("Luiz Agostini", ["luiz@webkit.org", "luiz.agostini@openbossa.org"], "lca"),
    Reviewer("Maciej Stachowiak", "mjs@apple.com", "othermaciej"),
    Reviewer("Mark Rowe", "mrowe@apple.com", "bdash"),
    Reviewer("Martin Robinson", ["mrobinson@webkit.org", "mrobinson@igalia.com", "martin.james.robinson@gmail.com"], "mrobinson"),
    Reviewer("Michael Saboff", "msaboff@apple.com", "msaboff"),
    Reviewer("Mihai Parparita", "mihaip@chromium.org", "mihaip"),
    Reviewer("Nate Chapin", "japhet@chromium.org", ["japhet", "natechapin"]),
    Reviewer("Nikolas Zimmermann", ["zimmermann@kde.org", "zimmermann@physik.rwth-aachen.de", "zimmermann@webkit.org", "nzimmermann@rim.com"], "wildfox"),
    Reviewer("Noam Rosenthal", "noam.rosenthal@nokia.com", "noamr"),
    Reviewer("Ojan Vafai", "ojan@chromium.org", "ojan"),
    Reviewer("Oliver Hunt", "oliver@apple.com", "olliej"),
    Reviewer("Pavel Feldman", ["pfeldman@chromium.org", "pfeldman@google.com"], "pfeldman"),
    Reviewer("Philippe Normand", ["pnormand@igalia.com", "philn@webkit.org", "philn@igalia.com"], ["philn-tp", "pnormand"]),
    Reviewer("Richard Williamson", "rjw@apple.com", "rjw"),
    Reviewer("Rob Buis", ["rwlbuis@gmail.com", "rwlbuis@webkit.org", "rbuis@rim.com"], "rwlbuis"),
    Reviewer("Ryosuke Niwa", "rniwa@webkit.org", "rniwa"),
    Reviewer("Sam Weinig", ["sam@webkit.org", "weinig@apple.com"], "weinig"),
    Reviewer("Shinichiro Hamaji", "hamaji@chromium.org", "hamaji"),
    Reviewer("Simon Fraser", "simon.fraser@apple.com", "smfr"),
    Reviewer("Simon Hausmann", ["hausmann@webkit.org", "hausmann@kde.org", "simon.hausmann@nokia.com"], "tronical"),
    Reviewer("Stephanie Lewis", "slewis@apple.com", "sundiamonde"),
    Reviewer("Stephen White", "senorblanco@chromium.org", "senorblanco"),
    Reviewer("Steve Block", "steveblock@google.com", "steveblock"),
    Reviewer("Steve Falkenburg", "sfalken@apple.com", "sfalken"),
    Reviewer("Tim Omernick", "timo@apple.com"),
    Reviewer("Timothy Hatcher", ["timothy@apple.com", "timothy@hatcher.name"], "xenon"),
    Reviewer("Tony Chang", "tony@chromium.org", "tony^work"),
    Reviewer("Tony Gentilcore", "tonyg@chromium.org", "tonyg-cr"),
    Reviewer(u"Tor Arne Vestb\u00f8", ["vestbo@webkit.org", "tor.arne.vestbo@nokia.com"], "torarne"),
    Reviewer("Vicki Murley", "vicki@apple.com"),
    Reviewer("Vsevolod Vlasov", "vsevik@chromium.org", "vsevik"),
    Reviewer("Xan Lopez", ["xan.lopez@gmail.com", "xan@gnome.org", "xan@webkit.org", "xlopez@igalia.com"], "xan"),
    Reviewer("Yury Semikhatsky", "yurys@chromium.org", "yurys"),
    Reviewer("Zack Rusin", "zack@kde.org", "zackr"),
    Reviewer("Zoltan Herczeg", ["zherczeg@webkit.org", "zherczeg@inf.u-szeged.hu"], "zherczeg"),
]

class CommitterList(object):

    # Committers and reviewers are passed in to allow easy testing
    def __init__(self,
                 committers=committers_unable_to_review,
                 reviewers=reviewers_list,
                 contributors=contributors_who_are_not_committers,
                 watchers=watchers_who_are_not_contributors):
        self._accounts = watchers + contributors + committers + reviewers
        self._contributors = contributors + committers + reviewers
        self._committers = committers + reviewers
        self._reviewers = reviewers
        self._contributors_by_name = {}
        self._accounts_by_email = {}
        self._accounts_by_login = {}

    def accounts(self):
        return self._accounts

    def contributors(self):
        return self._contributors

    def committers(self):
        return self._committers

    def reviewers(self):
        return self._reviewers

    def _name_to_contributor_map(self):
        if not len(self._contributors_by_name):
            for contributor in self._contributors:
                assert(contributor.full_name)
                assert(contributor.full_name.lower() not in self._contributors_by_name)  # We should never have duplicate names.
                self._contributors_by_name[contributor.full_name.lower()] = contributor
        return self._contributors_by_name

    def _email_to_account_map(self):
        if not len(self._accounts_by_email):
            for account in self._accounts:
                for email in account.emails:
                    assert(email not in self._accounts_by_email)  # We should never have duplicate emails.
                    self._accounts_by_email[email] = account
        return self._accounts_by_email

    def _login_to_account_map(self):
        if not len(self._accounts_by_login):
            for account in self._accounts:
                if account.emails:
                    login = account.bugzilla_email()
                    assert(login not in self._accounts_by_login)  # We should never have duplicate emails.
                    self._accounts_by_login[login] = account
        return self._accounts_by_login

    def _contributor_only(self, record):
        if record and not record.is_contributor:
            return None
        return record

    def _committer_only(self, record):
        if record and not record.can_commit:
            return None
        return record

    def _reviewer_only(self, record):
        if record and not record.can_review:
            return None
        return record

    def committer_by_name(self, name):
        return self._committer_only(self.contributor_by_name(name))

    def contributor_by_irc_nickname(self, irc_nickname):
        for contributor in self.contributors():
            # FIXME: This should do case-insensitive comparison or assert that all IRC nicknames are in lowercase
            if contributor.irc_nicknames and irc_nickname in contributor.irc_nicknames:
                return contributor
        return None

    def contributors_by_search_string(self, string):
        return filter(lambda contributor: contributor.contains_string(string), self.contributors())

    def contributors_by_email_username(self, string):
        string = string + '@'
        result = []
        for contributor in self.contributors():
            for email in contributor.emails:
                if email.startswith(string):
                    result.append(contributor)
                    break
        return result

    def _contributor_name_shorthands(self, contributor):
        if ' ' not in contributor.full_name:
            return []
        split_fullname = contributor.full_name.split()
        first_name = split_fullname[0]
        last_name = split_fullname[-1]
        return first_name, last_name, first_name + last_name[0], first_name + ' ' + last_name[0]

    def _tokenize_contributor_name(self, contributor):
        full_name_in_lowercase = contributor.full_name.lower()
        tokens = [full_name_in_lowercase] + full_name_in_lowercase.split()
        if contributor.irc_nicknames:
            return tokens + [nickname.lower() for nickname in contributor.irc_nicknames if len(nickname) > 5]
        return tokens

    def contributors_by_fuzzy_match(self, string):
        string_in_lowercase = string.lower()

        # 1. Exact match for fullname, email and irc_nicknames
        account = self.contributor_by_name(string_in_lowercase) or self.account_by_email(string_in_lowercase) or self.contributor_by_irc_nickname(string_in_lowercase)
        if account:
            return [account], 0

        # 2. Exact match for email username (before @)
        accounts = self.contributors_by_email_username(string_in_lowercase)
        if accounts and len(accounts) == 1:
            return accounts, 0

        # 3. Exact match for first name, last name, and first name + initial combinations such as "Dan B" and "Tim H"
        accounts = [contributor for contributor in self.contributors() if string in self._contributor_name_shorthands(contributor)]
        if accounts and len(accounts) == 1:
            return accounts, 0

        # 4. Finally, fuzzy-match using edit-distance
        string = string_in_lowercase
        contributorWithMinDistance = []
        minDistance = len(string) / 2 - 1
        for contributor in self.contributors():
            tokens = self._tokenize_contributor_name(contributor)
            editdistances = [edit_distance(token, string) for token in tokens if abs(len(token) - len(string)) <= minDistance]
            if not editdistances:
                continue
            distance = min(editdistances)
            if distance == minDistance:
                contributorWithMinDistance.append(contributor)
            elif distance < minDistance:
                contributorWithMinDistance = [contributor]
                minDistance = distance
        if not len(contributorWithMinDistance):
            return [], len(string)
        return contributorWithMinDistance, minDistance

    def account_by_login(self, login):
        return self._login_to_account_map().get(login.lower()) if login else None

    def account_by_email(self, email):
        return self._email_to_account_map().get(email.lower()) if email else None

    def contributor_by_name(self, name):
        return self._name_to_contributor_map().get(name.lower()) if name else None

    def contributor_by_email(self, email):
        return self._contributor_only(self.account_by_email(email))

    def committer_by_email(self, email):
        return self._committer_only(self.account_by_email(email))

    def reviewer_by_email(self, email):
        return self._reviewer_only(self.account_by_email(email))
