<?php
/**
 * Template Name: Site Map
 **/
?>
<?php get_header(); ?>

    <?php if (have_posts()) : while (have_posts()) : the_post(); ?>
        <style>
        article .menu a {
            color: hsl(0, 0%, 26.7%);
            color: var(--text-color-heading);
            text-decoration: none;
        }

        article .menu a:hover {
            color: hsl(200, 100%, 40%);
            color: var(--link-color);
        }

        article .menu,
        article .menu ul {
            list-style: none;
        }

        article .menu,
        article .sub-menu {
            padding-left: 0;
        }

        article .sub-menu li {
            display: inline-block;
            vertical-align: top;
            width: 32%;
            box-sizing: border-box;
        }

        article .sub-menu {
            padding-right: 1rem;
        }

        /* Top headings */
        article .menu > .menu-item-has-children > a {
            display: block;
            margin-top: 3rem;
            font-size: 3.2rem;
            line-height: 1.14286;
            font-weight: 200;
            letter-spacing: -0.01em;
            padding-bottom: 1rem;
            border-bottom: 1px solid hsl(0, 0%, 86.7%);
            border-color: var(--horizontal-rule-color);
            margin-bottom: 1rem;
        }

        /* Sub-section headings */
        article .sub-menu > .menu-item-has-children > a {
            display: block;
            font-weight: 600;
            margin-top: 2rem;
            margin-bottom: 1rem;
        }

        article .sub-menu .sub-menu li {
            display: block;
            width: 100%;
            margin-bottom: 0.5rem;
        }

        @media only screen and (max-width: 676px) {
            article .sub-menu li {
                width: 100%;
            }

        }
        </style>
        <article class="page sitemap" id="post-<?php the_ID(); ?>">

            <h1><a href="<?php echo get_permalink() ?>" rel="bookmark" title="Permanent Link: <?php the_title(); ?>"><?php the_title(); ?></a></h1>

            <div class="bodycopy">
            <?php wp_nav_menu( array('theme_location'  => 'sitemap') ); ?>
            </div>
        </article>

	<?php endwhile; else:
        include('444.php');
	endif; ?>

<?php get_footer(); ?>