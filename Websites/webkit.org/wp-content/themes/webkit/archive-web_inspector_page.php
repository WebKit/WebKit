<?php
    // Customized WP query for Web Inspector pages
    $query = new WP_Query(array(
        'post_type' => 'web_inspector_page',
        'post_status' => 'publish',
        'order' => 'ASC',
        'orderby' => 'title',
        'posts_per_page' => 999
    ));

    // Get the Web Inspector Topics taxonomy terms
    $terms = get_terms('web_inspector_topics', array(
        'hide_empty' => false
    ));

    get_header(); 
?>
    <style>
        :root {
            --feature-rule-color: hsl(0, 0%, 89.4%);
            --topic-color: hsl(0, 0%, 60%);
        }

        @media(prefers-color-scheme:dark) {
            :root {
                --feature-rule-color: hsl(0, 0%, 20%);
                --topic-color: hsl(0, 0%, 51%);
            }
        }

        .reference-filters {
            position: relative;
            box-sizing: border-box;
            width: 100vw;
            left: 50%;
            margin-bottom: 3rem;
            transform: translate(-50vw, 0);
            background-color: hsl(0, 0%, 0%);
            background-color: var(--figure-mattewhite-background-color);
            border: 1px solid hsl(0, 0%, 90.6%);
            border-color: var(--article-border-color);
            border-left: none;
            border-right: none;
        }

        .reference-filters form {
            position: relative;
            top: 0;
        }

        .reference-filters .search-input {
            width: 100%;
            box-sizing: border-box;
            margin-top: 0rem;
            margin-bottom: 0rem;
            padding: 1rem;
            padding-left: 3rem;
            padding-right: 8.5rem;
            background-repeat: no-repeat;
            background-position-x: 0.5rem;
            background-position-y: 1rem;
            background-size: 2rem;
            border-color: transparent;
            font-size: 2rem;
        }

        .reference-filters .filters-toggle-button {
            position: absolute;
            right: 3rem;
            top: 1rem;
            padding-right: 2.5rem;
            background-repeat: no-repeat;
            background-size: 2rem;
            background-position: right;
            background-filter: lightness(2);
            border: none;
            color: hsl(240, 2.3%, 56.7%);
        }

        .reference-filters .filters-toggle-button:hover {
            filter: brightness(0);
        }

        .reference-filters li {
            display: inline-block;
        }

        .reference-filters label {
            display: table-cell;
            float: right;
            padding: 0.5rem 1rem;
            border-style: solid;
            border-width: 1px;
            border-radius: 3px;
            cursor: pointer;
            font-size: 1.6rem;
            line-height: 1;
        }

        .topic-filters label {
            margin-left: 1rem;
            margin-bottom: 1rem;
        }

        .reference-filters label {
            float: none;
            display: inline-block;
        }

        .topic-filters {
            display: none;
            list-style: none;
            margin-top: 1rem;
            margin-bottom: 0.5rem;
            text-align: center;
        }

        #reference-filters.opened {
            margin-top: 1.5rem;
        }

        #reference-filters.opened .topic-filters {
            display: block;
        }
        #reference-filters.opened .search-input {
            border-color: hsl(0, 0%, 83.9%);
            border-color: var(--input-border-color);
        }

        .filter-toggle:checked + .filter-topic {
            color: hsl(240, 1.3%, 84.5%);
            color: var(--text-color);
        }

        .reference-filters label > input {
            position: relative;
            top: -1px;
        }

        .filter-topic {
            color: hsl(0, 0%, 60%);
            color: var(--topic-color);
            border-color: hsl(0, 0%, 60%);
            border-color: var(--topic-color);
        }

        .filter-topic {
            border-color: hsl(0, 0%, 60%);
            border-color: var(--topic-color)
        }

        .filter-toggle:checked + .filter-topic {
            background-color: hsl(0, 0%, 60%);
            background-color: var(--link-color);
        }

        #reference-list {
            column-count: 2;
            column-width: 50%;
            margin-bottom: 3rem;
        }

        #reference-list div {
            position: relative;
            margin-bottom: -1px;
            padding: 0.5rem;
            break-inside: avoid;
            border-color: transparent;
            border-width: 1px;
            border-style: solid;
            border-top-color: hsl(0, 0%, 89.4%);
            border-top-color: var(--feature-rule-color);
            border-bottom-color: hsl(0, 0%, 89.4%);
            border-bottom-color: var(--feature-rule-color);
            line-height: 1.618;
            transition: background-color 0.3s ease-out;
        }

        #reference-list .title {
            display: block;
            font-size: 2.2rem;
            font-weight: bold;
        }

        #reference-list li {
            display: inline;
            color: hsl(240, 2.3%, 56.7%);
            color: var(--text-color-coolgray);
            font-size: 1.4rem;
            text-align: left;
        }

        #reference-list li:not(:first-child):before {
            content: ', ';
        }

        #reference-list .hidden {
            display: none;
        }

        #reference-list + p {
            margin-bottom: 3rem;
        }

        @media(prefers-color-scheme:dark) {
            .search-input:hover,
            .search-input:focus,
            .reference-filters .filters-toggle-button:hover {
                filter: brightness(2);
            }
        }

        @media only screen and (max-width: 600px) {
            
            #reference-list {
                column-count: 1;
                column-width: 100%;
            }
            
            #reference-list div {
                width: 100%;
            }
            #reference-list div:nth-child(2) {
                border-top: none;
            }
        }
    </style>

    <h1>Web Inspector Reference</h1>

    <section class="reference-filters">
        <form id="reference-filters" class="page-width">
            <input type="text" id="search" class="search-input" placeholder="Search Web Inspector Reference&hellip;" title="Filter the reference articles." required><label class="filters-toggle-button">Filters</label>
            <ul id="topic-filters" class="topic-filters">
                <?php foreach($terms as $term): ?>
                    <li><label class="filter-topic <?php echo esc_html($term->slug); ?>" for="toggle-<?php echo esc_html($term->slug); ?>"><input type="checkbox" value="<?php echo esc_html($term->name); ?>" id="toggle-<?php echo esc_html($term->slug); ?>" class="filter-toggle"> <?php echo esc_html($term->name); ?></label></li>
                <?php endforeach ;?>
            </ul>
        </form>
    </section>

    <?php if (have_posts()): ?>
        <section id="reference-list">
            <?php while ($query->have_posts()) : $query->the_post();
                $postterms = get_the_terms(get_the_ID(), 'web_inspector_topics'); ?>
                <div class="reference"><a href="<?php the_permalink(); ?>" class="title"><?php the_title(); ?></a>
                    <ul><?php foreach ($postterms as $postterm) echo '<li class="topics">' . esc_html($postterm->name) . '</li>'; ?></div>
            <?php endwhile; ?>
        </section>
        <p>Can't find something? Contact <a href="https://twitter.com/webkit">@webkit</a> on Twitter and let the team know.</p>
    <?php endif; ?>

    <script>
        let filtersForm = document.getElementById('reference-filters');
        let topicFiltersList = document.getElementById('topic-filters');
        let filtersToggleButton = document.getElementsByClassName('filters-toggle-button')[0];
        let inputField = document.getElementById('search');
        let topicFilterToggles = topicFiltersList.getElementsByClassName('filter-toggle');
        let list = document.getElementById('reference-list');
        let referenceList = list.getElementsByClassName('reference');

        function filterReferenceList (e) {
            let searchTerm = inputField.value.trim().toLowerCase();
            let filteredTopics = topicFiltersList.querySelectorAll('.filter-toggle:checked');

            if (searchTerm.length === 0 && filteredTopics.length === 0) {
                for (const ref of referenceList)
                    ref.classList.remove('hidden');
                return;
            }

            for (const ref of referenceList) {
                ref.classList.add('hidden');
                for (const filteredTopic of filteredTopics) {
                    if (ref.getElementsByTagName('ul')[0].textContent.includes(filteredTopic.value)) {
                        ref.classList.remove('hidden');
                        break;
                    }
                }

                if (filteredTopics.length > 0 && ref.classList.contains('hidden'))
                    continue;

                if (searchTerm.length === 0)
                    continue;

                referenceText = ref.getElementsByClassName('title')[0].textContent + ref.getElementsByTagName('ul')[0].textContent;
                if (referenceText.toLowerCase().includes(searchTerm))
                    ref.classList.remove('hidden');
                else
                    ref.classList.add('hidden');
            }
            let activeTopics = [];
            for (const checkbox of filteredTopics.values())
                activeTopics.push(checkbox.value);
            updateURL(searchTerm, activeTopics);
        }

        function searchTermFromURL() {
            let search = window.location.search;
            let searchRegExp = /.*search=([^&]+)/;

            let result;
            if (result = window.location.href.match(searchRegExp))
                return decodeURIComponent(result[1]);

            return '';
        }

        function topicsFromURL() {
            let search = window.location.search;
            let topicRegExp = /\#.*topics=([^&]+)/;

            let result;
            if (result = window.location.href.match(topicRegExp))
                return result[1].split(',');

            return [];
        }

        function updateURL(searchTerm, activeTopicFilters) {
            let searchString = '';

            function appendDelimiter() {
                searchString += searchString.length ? '&' : '?';
            }

            if (searchTerm.length > 0) {
                appendDelimiter();
                searchString += 'search=' + encodeURIComponent(searchTerm);
            }

            if (activeTopicFilters.length > 0) {
                appendDelimiter();
                searchString += 'topics=' + activeTopicFilters.join(',');
            }

            if (searchString.length) {
                let current = window.location.href;
                window.location.href = current.replace(/\??#(.*)$/, '') + '#' + searchString;
            }
        }

        function toggleSearchInputPrompt(changed) {
            if (changed.matches)
                inputField.placeholder = 'Search…';
            else
                inputField.placeholder = 'Search Web Inspector Reference…';
        }

        (function() {
            filtersToggleButton.addEventListener('click', function(e) {
                filtersForm.classList.toggle('opened');
            });

            let placeholderMediaQuery = window.matchMedia('(max-width: 600px)');
            placeholderMediaQuery.addListener(toggleSearchInputPrompt);
            toggleSearchInputPrompt(placeholderMediaQuery);

            inputField.addEventListener('input', filterReferenceList);
            for (const toggle of topicFilterToggles)
                toggle.addEventListener('change', filterReferenceList);

            let searchTerm = searchTermFromURL();
            let topicFilters = topicsFromURL();
            for (const toggle of topicFilterToggles) {
                if (topicFilters.indexOf(toggle.value) !== -1) {
                    filtersForm.classList.add('opened');
                    toggle.checked = true;
                }
            }

            if (searchTerm.length) {
                inputField.value = searchTerm;
                inputField.placeholder = '';
                inputField.dispatchEvent(new InputEvent('input'));
            }
        }());
    </script>
<?php get_footer(); ?>