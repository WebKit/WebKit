	<div id="sidebar">
	<div id="sidebar_inner">
		<ul>
			<?php get_links_list('id'); ?>

			<li class="noseparator"><h2>Archives</h2>
				<ul>
				<?php wp_get_archives('type=monthly'); ?>
				</ul>
			</li>			
		</ul>
	</div>
	</div>

<script>
function setCurrentLink()
{
    var l = document.links;
    for (var i = 0; i < l.length; i++) {
        var a = l[i].href;
        var b = top.location.href;
        a = a.replace("file:///", "file:/");
        b = b.replace("file:///", "file:/");
        if (a == b) {
            l[i].className = "current";
            break;
        }
    }
}
setCurrentLink();
</script>
