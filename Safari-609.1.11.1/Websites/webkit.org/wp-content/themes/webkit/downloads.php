<?php
/**
 * Template Name: Downloads
 **/

add_filter('the_content', function ($content) {

    $build = get_nightly_build();
    $source = get_nightly_source();

    if ( empty($build) || empty($source) )
        return $content;

    $content = sprintf($content, $build[2], $source[2]);

    return $content;
});

add_filter('wp_head', function() { ?>

<style>
.page.downloads h1 {
    text-align: center;
    margin-bottom: 0;
}

.page.downloads blockquote:first-child {
    color: #8E8E93;
    text-align: center;
    font-size: 4rem;
    line-height: 6rem;
    font-weight: 200;
}

.bodycopy > div {
    box-sizing: border-box;
    padding: 0 1.5rem 3rem;
    width: 49%;
    text-align: center;
    display: inline-block;
}

#preview,
.bodycopy > .widescreen {
    float: none;
    width: 100%;
}

#nightly {
    float: none;
    position: relative;
    width: 100vw;
    left: 50%;
    transform: translate(-50%);
    background: #333333;
    padding-top: 3rem;
    color: #ffffff;
}

.page.downloads h2 {
    font-size: 3.2rem;
}

.downloads .download {
    font-size: 3rem;
}

@media only screen and (max-width: 920px) {
    .bodycopy > div {
        float: none;
        width: 100%;
    }
}
</style>
<?php
});

get_header();

        if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page downloads">
            <h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="bodycopy">
                <?php the_content(''); ?>
            </div>

        </article>

    <?php endwhile; endif; ?>

<?php get_footer(); ?>