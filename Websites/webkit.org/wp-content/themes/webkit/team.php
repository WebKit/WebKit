<?php
/**
 * Template Name: Team Page
 **/
?>
<?php get_header(); ?>
<style>
:root {
    --contributor-shadow: 0px 0px 10px 0px hsla(0, 0%, 0%, 0.1);
    --expertise-shadow: 0px 10px 10px 0px hsla(0, 0%, 0%, 0.1);
    --hover-text-color: hsl(0, 0%, 0%);
}

@media only screen and (prefers-color-scheme:dark) {
    :root {
        --contributor-shadow: 0px 0px 10px 0px hsla(0, 0%, 100%, 0.1);
        --expertise-shadow: 0px 5px 10px 0px hsla(0, 0%, 100%, 0.1);
        --hover-text-color: hsl(0, 0%, 100%);
    }
}
article ul {
    list-style: none;
    padding-left: 0;
    margin-left: -1rem;
    margin-top: 0;
}

article ul > li {
    position: relative;
    display: inline-block;
    vertical-align: top;
    width: 30%;
    padding: 1rem;
    margin-bottom: 1rem;
    border: 1px solid transparent;
    color: hsl(0, 0%, 33.3%);
    color: var(--text-color-medium);
}

li span,
li em {
    font-size: 1.3rem;
}

li em {
    display: block;
    line-height: 2rem;
}

.expertise {
    background-color: hsl(0, 0%, 100%);
    background-color: var(--button-background-color);
    box-shadow: 0px 10px 10px 0px hsla(0, 0%, 0%, 0.1);
    box-shadow: var(--expertise-shadow);

    position: absolute;
    font-size: 1.6rem;
    line-height: 2rem;
    margin-left: -1px;
    width: calc(100.3% + 2px);
    box-sizing: border-box;
    display: none;
    z-index: 100;
    color: hsl(0, 0%, 0%);
    color: var(--hover-text-color);
}

.bodycopy > ul > li:hover {
    background-color: hsl(0, 0%, 100%);
    background-color: var(--button-background-color);
    box-shadow: 0px 0px 10px 0px hsla(0, 0%, 0%, 0.1);
    box-shadow: var(--contributor-shadow);
    filter: none;
    color: hsl(0, 0%, 0%);
    color: var(--hover-text-color);
}

.bodycopy ul li:hover .expertise {
    filter: none;
    display: block;
}

@media only screen and (max-width: 675px) {

    article ul > li {
        width: 100%;
    }

}

</style>
	<?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page" id="post-<?php the_ID(); ?>">
			<h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

			<div id="team" class="bodycopy">
                <p><a href="#reviewers">Reviewers</a> | <a href="#committers">Committers</a> | <a href="#contributors">Contributors</a></p>

                <h2><a name="reviewers"></a>Reviewers</h2>
                <ul id="reviewers"></ul>

                <h2><a name="committers"></a>Committers</h2>
                <ul id="committers"></ul>

                <h2><a name="contributors"></a>Contributors</h2>
                <ul id="contributors"></ul>
			</div>
        </article>

	<?php //comments_template(); ?>

	<?php endwhile; else: ?>

		<p>No posts.</p>

	<?php endif; ?>

<script>

var svnTrunkUrl = 'https://svn.webkit.org/repository/webkit/trunk/';
var domainAffiliations = {
    'apple.com': 'Apple',
    'adobe.com': 'Adobe',
    'basyskom.com': 'basysKom GmbH',
    'cisco.com': 'Cisco Systems',
    'collabora.co.uk': 'Collabora',
    'company100.com': 'Company100',
    'google.com': 'Google',
    'igalia.com': 'Igalia',
    'intel.com' : 'Intel',
    'lge.com' : 'LG Electronics',
    'motorola.com': 'Motorola Mobility',
    'navercorp.com' : 'Naver',
    'nokia.com': 'Nokia',
    'openbossa.org': 'INdT / Nokia',
    'profusion.mobi': 'ProFUSION',
    'rim.com': 'Research In Motion',
    'samsung.com': 'Samsung Electronics',
    'sencha.com': 'Sencha',
    'sisa.samsung.com': 'Samsung Electronics',
    'sony.com': 'Sony',
    'tesla.com': 'Tesla',
    'torchmobile.com.cn': 'Torch Mobile (Beijing) Co. Ltd.',
    'digia.com': 'Digia',
    'partner.samsung.com': 'Samsung Electronics',

    // Universities
    'inf.u-szeged.hu': 'University of Szeged',
    'stud.u-szeged.hu': 'University of Szeged',

    // Open source communities
    'chromium.org': 'Chromium',
    'codeaurora.org': 'Code Aurora Forum',
    'gnome.org': 'GNOME',
    'kde.org': 'KDE'
};

function parseContributorsJSON(text) {
    var contributorsJSON = JSON.parse(text);
    var contributors = [];

    for (var contributor in contributorsJSON) {
        var data = contributorsJSON[contributor];
        if (data.class == "bot")
            continue;
        contributors.push({
            name: contributor,
            kind: data.status ? data.status : 'contributor',
            emails: data.emails,
            nicks: data.nicks,
            expertise: data.expertise
        });
    }
    return contributors;
}

function formatAffiliation(contributor) {
    if (contributor.affiliation)
        return contributor.affiliation;

    if (!contributor.emails || !contributor.emails.length)
        return null;

    var affiliations = [];
    for (var domain in domainAffiliations) {
        for (var i = 0; i < contributor.emails.length; i++) {
            if (contributor.emails[i].indexOf('@' + domain) > 0 && affiliations.indexOf(domainAffiliations[domain]) < 0)
                affiliations.push(domainAffiliations[domain]);
        }
    }
    return affiliations.join(' / ');
}

function addText(container, text) { container.appendChild(document.createTextNode(text)); }

function addWrappedText(container, tagName, attributes, text) {
    var element = document.createElement(tagName);
    for (var name in attributes)
        element.setAttribute(name, attributes[name]);
    addText(element, text);
    container.appendChild(element);
}

function populateContributorListItem(listItem, contributor) {
    addWrappedText(listItem, 'strong', {'class': 'name'}, contributor.name);
    if (contributor.nicks) {
        addWrappedText(listItem, 'span', {'class': 'nicks'}, ' (' + contributor.nicks.join(', ') + ')');
    }

    var affiliation = formatAffiliation(contributor);
    if (affiliation) {
        addText(listItem, ' ');
        addWrappedText(listItem, 'em', {'class': 'affiliation'}, affiliation);
    }

    if (contributor.expertise) {
        var expertiseList = document.createElement('ul');
        addWrappedText(expertiseList, 'li', {'class': 'expertise'}, contributor.expertise);
        listItem.appendChild(expertiseList);
    }
}

function populateContributorList(contributors, kind) {
    var contributorsOfKind = contributors.filter(function(contributor) { return contributor.kind == kind; });
    var listElement = document.getElementById(kind + 's');
    for (var i = 0; i < contributorsOfKind.length; i++) {
        var listItem = document.createElement('li');
        listElement.appendChild(listItem);
        populateContributorListItem(listItem, contributorsOfKind[i]);
    }
}

function nicksInListItem(listItem) {
    var nicksContainer = listItem.querySelector('.nicks');
    if (!nicksContainer || !nicksContainer.textContent)
        return null;
    return nicksContainer.textContent.split(/,\s*/);
}

function findListChildForContributor(contributor) {
    var listChildren = document.getElementsByTagName('li');
    for (var i = 0; i < listChildren.length; i++) {
        var nameContainer = listChildren[i].querySelector('.name');
        if (nameContainer && nameContainer.textContent.toLowerCase().indexOf(contributor.name.toLowerCase()) >= 0)
            return listChildren[i];
        var nicksInContainer = nicksInListItem(listChildren[i]);
        if (nicksInContainer && contributor.nicks) {
            for (var j = 0; j < contributor.nicks.length; j++) {
                if (nicksInContainer.indexOf(contributor.nicks[j]) >= 0)
                    return listChildren[i];
            }
        }
    }
    return null;
}

var xhr = new XMLHttpRequest();
xhr.onload = function () {
    if (this.status !== 200)
        return this.onerror();
    var contributors = parseContributorsJSON(this.responseText);

    populateContributorList(contributors, 'reviewer');
    populateContributorList(contributors, 'committer');
    populateContributorList(contributors, 'contributor');
};
xhr.onerror = function () { document.getElementById('team').textContent = 'There was an issue loading data for the WebKit Team. not obtain contributors.json'; };
xhr.open('GET', svnTrunkUrl + 'Tools/Scripts/webkitpy/common/config/contributors.json');
xhr.send();

</script>

<?php get_footer(); ?>
