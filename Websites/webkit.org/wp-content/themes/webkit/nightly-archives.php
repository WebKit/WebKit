<?php
/**
 * Template Name: Nightly Archives
 **/

$first_build = 11994;
$nightly_build = get_nightly_build();
$latest_build = $nightly_build[0];

add_action('wp_head', function() { ?>
    <script type="text/javascript">
    var ajaxurl = '<?php echo admin_url('admin-ajax.php'); ?>';
    </script>
<?php
});

add_filter('the_content', function ($content) {

    $archives = get_nightly_archives(60);

    ob_start();
    foreach ($archives as $record): ?>
<li>
    <a href="<?php echo esc_url($record[2]); ?>"><?php echo "r" . $record[0]; ?></a> <span class="date"><?php echo $record[1]; ?></span>
</li>
<?php endforeach;
    $list = ob_get_clean();

    $content = "<div id=\"search-errors\"></div><div id=\"search-results\" class=\"multi-column\"><ul>$list</ul></div>";

    return $content;
});

add_action( 'wp_enqueue_scripts', function() {
    wp_enqueue_script( 'searchbuilds', get_template_directory_uri() . '/scripts/searchbuilds.js', array(), '1.0.0', true );
});

get_header();
?>
<style>
#archives h1 {
    text-align: center;
}

#downloads blockquote:first-child {
    color: #8E8E93;
    text-align: center;
    font-size: 4rem;
    line-height: 6rem;
    font-weight: 200;
}

#downloads .bodycopy > div {
    box-sizing: border-box;
    padding: 0 1.5rem 3rem;
    width: 50%;
    text-align: center;
    float: left;
}

#downloads h2 {
    font-size: 3.2rem;
}

li a {
    display: block;
    line-height: 2rem;
}

li span {
    font-size: 1.6rem;
    color: #8E8E93;
}

.multi-column {
    -webkit-column-count:3;
    -moz-column-count:3;
    column-count:3;
    -webkit-column-gap:3rem;
    -moz-column-gap:3rem;
    column-gap:3rem;
    columns:3;
}

.bodycopy ul {
    list-style: none;
    margin: 0;
    padding: 0;
}

.bodycopy ul > li {
    display: inline-block;
    width: 100%;
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

</style>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page" id="archives">

            <h1><?php before_the_title(); ?><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="bodycopy">
                <?php if ($nightly_build > 0): ?>
                <div class="search">
                    <input type="text" placeholder="Search builds: <?php echo esc_attr($first_build); ?>-<?php echo esc_attr($latest_build); ?>" class="buildsearch">
                    <?php include('images/spinner.svg'); ?>
                </div>
                <?php else: ?>
                <div class="note"><h2>Error</h2> <p>There was an problem loading the build archive data.</p></div>
                <?php endif; ?>
                <?php the_content(); ?>
            </div>

        </article>

    <?php endwhile; endif; ?>

<script type="text/javascript">
    var builds = <?php echo get_nightly_builds_json(); ?>;
</script>

<?php get_footer(); ?>