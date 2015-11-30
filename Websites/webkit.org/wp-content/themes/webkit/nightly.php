<?php
/**
 * Template Name: Nightly Downloads
 **/

define('WEBKIT_NIGHTLY_ARCHIVE_URL', "http://nightly.webkit.org/builds/trunk/%s/all");

function get_nightly_build() {
    return get_nightly_download_details('mac');
}

function get_nightly_source() {
    return get_nightly_download_details('src');
}

function get_nightly_download_details( $type = 'mac' ) {
    $types = array('mac', 'src');
    if ( ! in_array($type, $types) ) 
        $type = $types[0];
    
    $cachekey = 'nightly_download_' . $type;
    if ( false !== ( $cached = get_transient($cachekey) ) )
        return json_decode($cached);
	
    $url = sprintf(WEBKIT_NIGHTLY_ARCHIVE_URL, $type);
    $resource = fopen($url, 'r');
    $rawdata = fread($resource, 128);
    list($data,) = explode("\n", $rawdata);
    fclose($resource);
    
    if ( ! empty($data) ) {
        $record = explode(',', $data);
        set_transient($cachekey, json_encode($record), DAY_IN_SECONDS);
        return $record;
	}
    
    return false;
    
}

add_filter('the_content', function ($content) {
        
    $build = get_nightly_build();
    $source = get_nightly_source();
    
    $content = sprintf($content, 
                        
        $build[0], 
        date(get_option( 'date_format' ), $build[1]),
        $build[2],
    
        $source[0],
        date(get_option( 'date_format' ), $source[1]),
        $source[2]
                            
    );
    
    return $content;
});

get_header();
?>
<style>
body {
    background: #333333;
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

#nightly img {
    width: 33%;
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