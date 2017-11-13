<?php
/**
 * Template Name: Nightly Downloads
 **/

add_filter('the_content', function ($content) {

    $build = get_nightly_build();
    $source = get_nightly_source();

    $content = sprintf($content,
        $build[0], $build[1], $build[2],
        $source[0], $source[1], $source[2]
    );

    return $content;
});

get_header();
?>
<style>
body {
    background: #333333;
}

main {
    background: none;
}

header {
    background-color: rgba(0,0,0,0.1);
}

.page-color {
    /*background: #333333;*/
    background: linear-gradient(black, #333333 66%);
}

#nightly {
    margin: 6rem auto;
    text-align: center;
}

#nightly h1 {
    text-align: center;
    margin-bottom: 0;
}

#nightly h1 a {
    color: #ffffff;
    font-weight: 100;
    font-size: 9rem;
    line-height: 9rem;
}

#nightly p {
    text-align: center;
    color: #D39E23;
}

#nightly blockquote:first-child {
    color: #ffffff;
    text-align: center;
    font-size: 3rem;
    line-height: 4.2rem;
    font-weight: 200;
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
    background: none;
    padding-right: 0;
}

.page-template-nightly hr {
    border-color: #999;
}

.page-template-nightly #footer-nav a {
    color: #999;
}
</style>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>

        <article class="page" id="nightly">
            <h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="bodycopy">
                <?php the_content(''); ?>
            </div>

        </article>

    <?php endwhile; endif; ?>

<?php get_footer(); ?>