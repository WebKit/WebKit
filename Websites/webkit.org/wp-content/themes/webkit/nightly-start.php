<?php
/**
 * Template Name: Nightly Start Page
 **/

$latest_nightly_build = get_nightly_build();
$latest = intval($latest_nightly_build[0]);

$branch = get_query_var('nightly_branch');
$build = intval(get_query_var('nightly_build'));

if ($build < 11976 || empty($branch))
    header('Location: /not-found');

get_header();


add_filter('the_content', function ($content) {
    $branch = get_query_var('nightly_branch');
    $build = intval(get_query_var('nightly_build'));

    $content = sprintf($content, "r$build", $branch);
    
    if (empty($build))
        $content = str_replace('Running build r0', '', $content);
    
    return $content;

});
?>
<style>
body {
    background: #333333;
}

#nightly {
    margin: 6rem auto;
    color: #ffffff;
}

#nightly h1 {
    text-align: center;
    margin-bottom: 0;
    color: #ffffff;
    font-weight: 100;
    font-size: 9rem;
    line-height: 9rem;
    margin-top: 0;
}

#nightly h1 + blockquote {
    color: #FEC84C;
    text-align: center;
    font-size: 3rem;
    line-height: 4.2rem;
    font-weight: 200;
}

#nightly img {
    width: 33%;
}

#nightly .bodycopy > p:nth-child(3) {
    text-align: right;
}

#nightly blockquote:first-child p {
    color: #FFD15E;
}

#nightly a {
    color: #edd291;
}

#nightly a:hover {
    color: #ffffff;
}

#nightly a.download {
    color: #ffffff;
    font-size: 3rem;
}

hr {
    border-color: #777777;
}

#footer-nav a {
    color: #999999;
}

.update-nag {
    background-color: #FF9D00;
    color: #000000;
    width: 100vw;
    left: 50%;
    position: absolute;
    transform: translate(-50vw);
    padding: 1.5rem;
    padding-top: 14.5rem;
    top: 0;
}

.update-nag p {
    text-align: center;
    font-size: 2rem;
}

.update-nag + #nightly {
    margin-top: 9rem;
}

</style>
<?php if ( (int)$build < (int)$latest ): ?>
<div class="update-nag">
    <p><?php $prompt_meta = get_post_meta(get_the_ID(), 'prompt'); echo esc_html($prompt_meta[0]); ?></p>
</div>
<?php endif; ?>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page" id="nightly">
            
            <div class="bodycopy">
                <?php the_content(''); ?>
            </div>
            
        </article>

    <?php endwhile; endif; ?>

<?php get_footer(); ?>