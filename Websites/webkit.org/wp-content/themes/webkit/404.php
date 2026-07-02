<?php get_header(); ?>
<style>
main h1 {
    font-size: 6rem;
    padding-top: 9rem;
    text-align: left;
}
h1, p, form {
    margin-bottom: 3rem;
}
img {
    float: right;
    margin-top: -3rem;
    width: 50vw;
    max-width: 800px;
}
input {
    height: 3.2rem;
    padding-top: 3px;
    padding-left: 1.5rem;
    padding-right: 1.5rem;
}
</style>

<img src="<?php echo get_stylesheet_directory_uri(); ?>/images/squirrelfish-lives.svg" class="alignright">
<h1>Not Found</h1>

<p>Sorry this isn't the page you're looking for.</p>

<p>You can try a search or return to the <a href="<?php bloginfo('siteurl'); ?>">home page</a>.</p>

<?php echo get_search_form(); ?>

<?php get_footer(); ?>