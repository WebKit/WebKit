from buildbot.status import html, mail, words

web = html.WebStatus(http_port=8010)

allBuildsEmail = mail.MailNotifier(fromaddr="buildbot@webkit.org",
                                    extraRecipients=["mark+webkit-builds@bdash.net.nz"],
                                    sendToInterestedUsers=False)
breakageEmail = mail.MailNotifier(fromaddr="buildbot@webkit.org",
                                  lookup=mail.Domain("webkit.org"),
                                  mode="failing")

IRC = words.IRC(host="irc.freenode.net",
                nick="webkit-build",
                channels=["#webkit-build"],
#                announceAllBuilds=True
                )

def getStatusListeners():
    return [web, allBuildsEmail, IRC]
