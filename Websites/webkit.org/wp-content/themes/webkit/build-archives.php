<?php
/**
 * Template Name: Build Archives
 **/

WebKitBuildArchives::object();

class WebKitBuildArchives {

    private static $object = null;

    public static $platforms = array(
        'mac-highsierra-x86_64' => 'High Sierra',
        'mac-sierra-x86_64'     => 'Sierra',
        'mac-elcapitan-x86_64'  => 'El Capitan',
    );

    public static function object() {
        if (self::$object === null)
            self::$object = new self();
        return self::$object;
    }

    private function call ($endpoint, $params = array()) {
        $url = add_query_arg($params, 'https://q1tzqfy48e.execute-api.us-west-2.amazonaws.com/v2/' . $endpoint);
        $api = wp_remote_get($url);
        $response = wp_remote_retrieve_body($api);

        if (is_wp_error($response))
            return $response;

        return json_decode($response);
    }

    public function get_latest($platform_key) {
        $latest = array();
        $cachekey = 'webkit_build_archives_latest_' . $platform_key;

        if (false !== ($cached = get_transient($cachekey)))
                    return unserialize($cached);

        $data = $this->call("latest/$platform_key-release");

        $latest[$platform_key] = array();

        foreach ($data->Items as &$entry) {
            $revision = new stdClass();
            $revision->url = $entry->s3_url->S;
            $revision->creationTime = $entry->creationTime->N;
            $latest[$platform_key]["r" . $entry->revision->N] = $revision;
        }

        set_transient($cachekey, serialize($latest), 600); // expire cache every 10 minutes

        return $latest;
    }

} // class WebKitBuildArchives

add_action('wp_head', function() { ?>
    <script type="text/javascript">
    (function(document) {
        document.addEventListener("DOMContentLoaded", function () {

            var creationTimeNodes = Array.prototype.slice.call(document.getElementsByClassName("date"));
            for (var timestamp of creationTimeNodes) {
                var date = new Date(parseInt(timestamp.textContent));
                timestamp.textContent = date.toLocaleDateString("en", {
                    "timeZoneName": "short",
                    "minute":       "2-digit",
                    "hour":         "2-digit",
                    "day":          "numeric",
                    "month":        "long",
                    "year":         "numeric"
                })
            }

            var tabnav = Array.prototype.slice.call(document.getElementsByClassName("tabnav-link")),
                currentTab = function(e) {
                    var target = e.target ? e.target : e,
                        currentLink = document.getElementsByClassName("tabnav-link current")
                    if (currentLink.length)
                        currentLink[0].classList.remove("current");
                    target.classList.add("current");

                    var results = document.getElementById("results"),
                        currentList = results.getElementsByClassName("current");
                    if (currentList.length)
                        currentList[0].classList.remove("current");

                    var list = results.getElementsByClassName(target.classList.item(1))[0];
                    list.classList.add("current");
                };

            var currentHash = window.location.hash.length ? window.location.hash.replace("#", "") : "mac-highsierra-x86_64";
            for (var link of tabnav) {
                link.addEventListener("click", currentTab);
                if (link.className.indexOf(currentHash) !== -1)
                    currentTab(link);
            }

        });
    }(document))
    </script>
<?php
});

add_action('wp_head', function() {
    echo '<meta name="robots" content="nofollow">';
});

add_filter('the_content', function ($content) {
    $API = WebKitBuildArchives::object();

    $error_markup = '<div class="note"><h2>Error</h2> <p>There was an problem loading the build archives data.</p></div>';

    $archives = array();
    $tabs = '<nav class="tabnav"><ul class="tabnav-items">';
    foreach (WebKitBuildArchives::$platforms as $platform => $label) {
        $platform_latest = $API->get_latest($platform);
        if (!empty($platform_latest)) {
            $archives = array_merge($archives, $platform_latest);
            $tabs .= '<li class="tabnav-item"><a href="#' . esc_attr($platform) . '" class="tabnav-link ' . esc_attr($platform) . '">' . $label . '</a></li>';
        }
    }

    $tabs .= '</ul></nav>';

    if (empty($archives))
        return $error_markup;

    $lists = '';
    ob_start();
    foreach ($archives as $platform => $revisions):

        if (empty($revisions)) {
            echo '<div class="platform-items ' . esc_attr($platform) . '">' . $error_markup . '</div>';
            continue;
        }
    ?>

    <ul class="platform-items <?php echo esc_attr($platform); ?>">
        <?php foreach ($revisions as $revision => $entry): ?>
        <li>
            <h6><a href="<?php echo esc_url($entry->url); ?>"><?php echo $revision; ?></a></h6>
            <span class="date"><?php echo intval($entry->creationTime) * 1000; ?></span>
        </li>
        <?php endforeach?>
    </ul>
<?php endforeach;
    $lists .= ob_get_clean();

    $content = $tabs . "<div id=\"search-errors\"></div><div id=\"results\">$lists</div>";

    return $content;
});

get_header();
?>
    <style>
    #archives h1 {
        text-align: center;
    }

    .bodycopy ul > li {
        line-height: 1;
    }

    #results .date {
        display: block;
        border: none;
        font-size: 1.4rem;
        text-transform: uppercase;
        padding-left: 0;
        line-height: 3rem;
        color: hsl(0, 0%, 87%);
        color: var(--text-color-light);
    }

    .bodycopy ul {
        list-style: none;
        margin: 0;
        padding: 0;
    }

    .platform-items li {
        display: inline-block;
        flex: 1;
        min-width: 33%;
        margin-bottom: 3rem;
    }

    .bodycopy .search {
        position: relative;
        text-align: center;
    }

    .search {
        position: relative;
    }

    .search input {
        width: 60%;
        position: relative;
        left: 2.25rem;
        padding-right: 4rem;
    }

    #search-spinner {
        left: -2.25rem;
        position: relative;
        width: 3rem;
        height: 3rem;
        padding: 0.5rem;
        visibility: hidden;
    }

    #search-spinner.searching {
        visibility: visible;
    }

    .tabnav {
        margin-top: 0px;
        margin-right: auto;
        margin-bottom: 3rem;
        margin-left: auto;
        padding-top: 0px;
        padding-right: 0px;
        padding-bottom: 0px;
        padding-left: 0px;
        width: 100%;
        text-align: center;
        position: relative;
        white-space: nowrap;
        overflow-x: auto;
        overflow-y: hidden;
    }

    .tabnav-items {
        display: inline-block;
        margin-top: 0px;
        margin-right: 0px;
        margin-bottom: 0px;
        margin-left: 0px;
    }

    .tabnav-item {
        padding-left: 60px;
        border-bottom-width: 1px;
        border-bottom-style: solid;
        border-bottom-color: hsl(0, 0%, 86.7%);
        border-bottom-color: var(--horizontal-rule-color);
        display: inline-block;
        list-style-type: none;
        list-style-position: initial;
        list-style-image: initial;
        outline-color: initial;
        outline-style: none;
        outline-width: initial;
    }

    .tabnav-item:first-child {
        padding-left: 0px;
    }

    .tabnav-link {
        font-size: 1.7rem;
        line-height: 1;
        font-weight: 400;
        letter-spacing: -0.021rem;
        padding-top: 1.1rem;
        padding-right: 0px;
        padding-bottom: 1.1rem;
        padding-left: 0px;
        color: rgb(102, 102, 102);
        text-align: left;
        text-decoration: none;
        display: block;
        margin-bottom: 0.4rem;
        position: relative;
        z-index: 0;
    }

    .tabnav-link.current {
        pointer-events: none;
        color: hsl(0, 0%, 26.7%);
        color: var(--text-color-heading);
        text-decoration: none;
        cursor: default;
        z-index: 10;
    }

    .tabnav-link.current::after {
        left: 0px;
        position: absolute;
        bottom: -5px;
        width: 100%;
        border-bottom-width: 1px;
        border-bottom-style: solid;
        border-bottom-color: hsl(0, 0%, 26.7%);
        border-bottom-color: var(--text-color-heading);
        content: "";
    }

    .tabnav-link:hover {
        color: hsl(200, 100%, 40%);
        color: var(--link-color);
        text-decoration: none;
    }

    .platform-items {
        display: none;
    }

    .platform-items.current {
        display: flex;
        flex-wrap: wrap;
    }

    .platform-items.current .note {
        flex-grow: 1;
    }
    </style>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page" id="archives">

            <h1><?php before_the_title(); ?><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="bodycopy">
                <?php the_content(); ?>
            </div>

        </article>

    <?php endwhile; endif; ?>

<?php get_footer(); ?>