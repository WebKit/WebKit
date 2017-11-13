<?php get_header(); ?>
<style>
h1 {
    font-size: 6rem;
    font-weight: 100;
    padding-top: 9rem;
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
    display: inline-block;
    box-sizing: border-box;
    vertical-align: top;
    height: 32px;
    padding-top: 3px;
    margin-bottom: 16px;
    padding-left: 15px;
    padding-right: 15px;
    font-size: 15px;
    color: #333333;
    text-align: left;
    border: 1px solid #d6d6d6;
    border-radius: 4px;
    background: white;
    background-clip: padding-box;
    font-family: "Myriad Set Pro", "Helvetica Neue", "Helvetica", "Arial", sans-serif;
    font-size: 15px;
    line-height: 1.33333;
    font-weight: 400;
    letter-spacing: normal;
    font-size: 2rem;
}

input[type=submit] {
    background-color: #1d9bd9;
    background: linear-gradient(#3baee7, #0088cc);
    border-radius: 4px;
    color: white;
    cursor: pointer;
    font-size: 1.5rem;
    font-weight: 500;
    text-align: center;
    border: 0;
}
</style>

<img src="<?php echo get_stylesheet_directory_uri(); ?>/images/squirrelfish-lives.svg" class="alignright">
<h1>Not Found</h1>

<p>Sorry this isn't the page you're looking for.</p>

<p>You can try a search or return to the <a href="<?php bloginfo('siteurl'); ?>">home page</a>.</p>

<?php echo get_search_form(); ?>


<?php get_footer(); ?>