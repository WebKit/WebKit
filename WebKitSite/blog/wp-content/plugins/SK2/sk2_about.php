<div class="sk_first wrap sk2_about">
<blockquote><p><i>Lasciate ogni speranza, voi ch'entrate</i></p>
<p class="source"><?php _e("Some dead Italian dude", 'sk2'); ?></p></blockquote>
<blockquote><p><i><?php _e("Hey... Wanna play some poker?", 'sk2'); ?></i></p>
<p class="source"><?php _e("Some Internet jackass", 'sk2'); ?></p></blockquote>
<br/>
<h2 class="sk_first"><?php _e("The Machine", 'sk2'); ?></h2>
<p><strong><?php _e('<a href="http://unknowngenius.com/blog/wordpress/spam-karma" target="_blank">Spam Karma 2</a></strong> is the proud successor to Spam Karma, with whom it shares most of the development ideas, but absolutely none of the code. It is meant to stop all forms of automated Blog spam effortlessly, while remaining as unobtrusive as possible to regular commenters.', 'sk2');?></p>

<h2><?php _e("The Man", 'sk2'); ?></h2>
<p><strong><?php _e('<a href="http://unknowngenius.com/blog/cast/me/" target="_blank">Dr Dave</a></strong> is a card-carrying Evil Genius still busy plotting world domination from his secret hideout in the Parisian catacombs (having recently relocated from his volcanic lair in Tokyo, Japan).
When he is not wasting his time coding ridiculously elaborate anti-spam plugins, he also writes somewhat offensive entries on miscellaneous pointless matters in <a href="http://unknowngenius.com/blog/" target="_blank">his blog</a>.', 'sk2'); ?></p>

<h2><?php _e("The Bribes", 'sk2'); ?></h2>
<p><?php _e('Donations are in <em>no way</em> mandatory.', 'sk2'); ?></p>
<p><?php _e('Yet, I have been spending considerable amounts of time writing, supporting and debugging both SK1 and SK2. Therefore, if you like what you see, and want to see more of it in the future, feel free to help me finance my many costly addictions by droping a buck or two in the tip jar (<a href="http://unknowngenius.com/blog/archives/2006/01/30/the-state-of-spam-karma/#donations">more reasons why you should</a>)...', 'sk2'); ?></p>
<p><form action="https://www.paypal.com/cgi-bin/webscr" method="post" style="text-align:center;">
<input type="radio" name="amount" value="2.00" checked="checked"/>$2.00<br />
<input type="radio" name="amount" value="5.00"/>$5.00<br />
<input type="radio" name="amount" value="10.00"/>$10.00<br />
<input type="radio" name="amount" value="20.00"/>$20.00<br />
<input type="radio" name="amount" value="30.00"/>$30.00<br />
<input type="radio" name="amount" value="50.00"/>$50.00<br />
<input type="radio" name="amount" value="666.00"/>$666.00<br />
<input type="hidden" name="cmd" value="_xclick"/><input type="hidden" name="business" value="paypal@unknowngenius.com"><input type="hidden" name="item_name" value="Buy Dr Dave a Lifetime supply of Bombay Sapphire Fund"/><input type="hidden" name="no_shipping" value="1"/><input type="hidden" name="return" value="http://unknowngenius.com/blog/" /><input type="hidden" name="cancel_return" value="http://unknowngenius.com/blog/"/><input type="hidden" name="currency_code" value="USD"/><input type="hidden" name="tax" value="0"/><input type="hidden" name="bn" value="PP-DonationsBF"/><input type="image" src="https://www.paypal.com/en_US/i/btn/x-click-but21.gif" border="0" name="submit" alt="Make payments with PayPal - it's fast, free and secure!" style="border: none;"/></form>
</p>
<p><?php _e('On behalf of Dr Dave Inc., I present you with our guarantee that no less than 25% of all proceedings will go directly toward the purchase of the finest <a href="http://www.physics.uq.edu.au/people/nieminen/bombay_sapphire.html" target="_blank">Bombay Sapphire</a> this city has to offer.', 'sk2'); ?></p>
<p><?php _e('Kind words of support or constructive comments are always welcome as well. Please either use the comment form on <a href="http://unknowngenius.com/blog/wordpress/spam-karma/">SK2\'s</a> page or contact me directly at: <a href="mailto:sk2@unknowngenius.com">sk2@unknowngenius.com</a>.', 'sk2'); ?></p>
<h2><?php _e("The Cast", 'sk2'); ?></h2>
<p><?php _e("Many, many people have, one way or another, contributed to Spam Karma... Including, in alphabetical order:", 'sk2'); ?></p>
<fieldset class="sk2_cast"><legend><b><?php _e("Mad Coding Skillz", 'sk2'); ?></b></legend>
<p><?php
	$coders = array("James Seward" => "http://www.grooblehonk.co.uk/",
"Matt Read" => "http://mattread.com/",
"Mark Jaquith" => "http://txfx.net/category/internet/wordpress/",
"Peter Westwood" => "http://blog.ftwr.co.uk/",
"Owen Winkler" => "http://www.asymptomatic.net/",
"Jason" => "http://ushimitsudoki.com/",
"Drac" => "http://fierydragon.org",
);

	ksort($coders);
	$i = count($coders);
	foreach ($coders as $name => $url)
	{
		echo "<a href=\"$url\">$name</a>";
		$i--;
		if ($i == 1)
			echo " & ";
		elseif ($i)
			echo ", ";
	
	}
?>
</p></fieldset>


<fieldset class="sk2_cast"><legend><b>International Team</b></legend>
<p><?php
	$coders = array(
	"Tsuyoshi Fukuda" => array("http://tsulog.com", "jp_JP"),
	"Xavier Borderie" => array("http://xavier.borderie.net/blog/", "fr_FR"),
	"BlueValentine" => array("http://www.AlchemicoBlu.it", "it_IT"),
	"Pal" => array("", "zh_CN"),
	"Marcel Bischoff" => array("", "de_DE"),
	"Anne" => array("", "nl_NL"),
	"Joshua Sigar" => array("", "di_DI"),
	"César Gómez Martín" => array("http://www.cesar.tk/", "es_ES"),
	"Jan Bauer" => array("", "de_DE"),
	"Michael Boman" => array("", "sv_SE"),
	"Johan Folin" => array("", "sv_SE"),
	"Dafydd Tomos" => array("http://da.fydd.org/blog/", "cy_GB"),
	);

	ksort($coders);
	$i = count($coders);
	foreach ($coders as $name => $info)
	{
		if ($info[1] == WPLANG)
			echo "<strong>";
		if ($info[0])
			echo "<a href=\"$info[0]\">$name</a>";
		else
			echo "$name";
		echo " (". (substr($info[1], 0, 2)) . ")";
		if ($info[1] == WPLANG)
			echo "</strong>";
		
		$i--;
		if ($i == 1)
			echo " & ";
		elseif ($i)
			echo ", ";
	
	}
	echo "</p><p><em>Please email me your name+URL+language if you have contributed to SK2's l10n effort</em>";
?>
</p></fieldset>

<fieldset class="sk2_cast"><legend><b><?php _e("Fearless Guinea Pigs, Generous Souls and All Around Cool People", 'sk2'); ?></b></legend>
<p><?php
	$testers = array("Stephanie Booth" => "http://climbtothestars.org/",
"Chris Davis" => "http://chrisjdavis.org/",
"Admiral Justin" => "http://s89631316.onlinehome.us/",
"Gustavo Barrón" => "http://blog.idealabs.tk/",
"Scott Reilly" => "http://www.coffee2code.com/",
"Jennifer" => "http://www.geeksmakemehot.com/",
"Alex Smith" => "http://blog.alexsmith.org/",
"Michel Valdrighi" => "http://zengun.org/weblog/",
"Peter Nacken" => "http://nacken.com/",
"Neuro" => "http://www.eretzvaju.org/",
"Chris Coggburn" => "http://chris.coggburn.us/",
"Firas" => "http://firasd.org",
"Samir Nassar" => "http://steamedpenguin.com/",
"Twidget" => "http://charlesstricklin.com/",
"Jon Abad" => "http://www.jonabad.com/",
"Chris" => "http://serendipity.lascribe.net/",
"Lisa Williams" => "http://www.cadence90.com/wp/index.php",
"Chris Kelly" => "http://www.ckelly.net/journal/",
"Matthew Mullenweg" => "http://photomatt.net/",
"Deamos" => "http://thetriadofdestruction.com/",
"MacManX" => "http://www.macmanx.com/wordpress",
"Podz" => "http://www.tamba2.org.uk/T2/",
"Harry Teasley" => "http://www.factoryofinfinitebliss.com",
"Nikkiana" => "http://www.everytomorrow.org/",
"Jennifer" => "http://www.geeksmakemehot.com/",
"Steve Rider" => "http://my-daily-rant.com/",
"Melanie" => "http://itcouldbenothing.com/fruitfly/",
"Nicki" => "http://www.nixit.co.nz/wordpress/",
"John Robinson" => "http://www.thebeard.org/weblog.html",
"Jayne Maxwell" => "",
"Thomas Cloer" => "http://www.teezeh.info",
"Tony" => "http://tony.brilliant-talkers.com/wp",
"Mog" => "http://www.mindofmog.net",
"Geof Morris" => "http://gfmorris.net",
"Jonathan Dingman" => "http://www.soundlogic.us/",
"Pieterjan Lansbegen" => "http://pjl.ath.cx",
"Carsten Stracke" => "http://www.subaquasternalrubs.com/",
"Will Hines" => "http://www.spitemag.com/",
"Indranil" => "http://design.troidus.com/",
"Michel Dumais" => "http://www.micheldumais.com/",
"Phil Hodgen" => "http://hodgen.com/",
"Stephan Lamprecht" => "http://news.lamprecht.net/",
"Robbie C." => "http://urbangrounds.com/category/about/",
"Michael Thomsen" => "http://www.blindmindseye.com",
"Russ" => "http://russ.innereyes.com",
"Jabley" => "http://www.jabley.com/",
"Glutnix" => "http://www.webfroot.co.nz",
"Christoph Schmalz" => "http://schmalz.net/",
"Sharon Howard" => "http://www.earlymodernweb.org.uk/emn/",
"Maria" => "http://intueri.org/",
"Steve Scaturan" => "http://negimaki.com",
"Ben Margolin" => "http://blog.benjolo.com/",
"Mr. Bill" => "http://www.mrbill.net/",
"David Gibbs" => "http://david.fallingrock.net/",
"Aly de Villers" => "http://www.conejoaureo.com/",
"Stewart Russell" => "http://scruss.com/blog/",
"Gonéri Le Bouder" => "http://alice.rulezlan.org/blog/",
"MeeCiteeWurkor" => "http://meeciteewurkor.com/wp/",
"Dan Milward" => "http://instinct.co.nz/",
"Viper007bond " => "http://www.viper007bond.com",
"Ajay D'Souza" => "http://www.ajaydsouza.com",
"Brian Epps" => "http://randomnumbers.us/",
"Chris Church" => "http://www.blahgkarma.com/wp/",
"Rob Lund" => "http://electrolund.com",
"Chris Teodorski" => "http://chris.teodorski.com",
"Bear Shirt Press" => "http://www.bearshirtpress.com/",
"Sabine Kirstein" => "",
"Selig Landsman" => "",
"Christian Mohn" => "http://h0bbel.p0ggel.org",
"Tobias Klausmann" => "http://schwarzvogel.de",
"Rodolpho Carrasco" => "http://www.urbanonramps.com",
"Kog Marketing" => "http://www.kogmarketing.com/",
"Aine" => "http://evilqueen.demesnes.net/",
"Maria Langer" => "http://www.marialanger.com",
"Miklb" => "http://www.miklb.com/",
"S. W. Anderson" => "http://wpblog.ohpinion.com/",
"Bonnie Wren" => "http://www.bonniewren.com/",
"Larry Fransson" => "http://www.subcritical.com/",
"Sean Carroll" => "http://cosmicvariance.com/",
"Josh Larios" => "http://www.elsewhere.org/journal/",
"Etanisla Lopez-Ortiz" => "http://www.carelessthought.com/",
"Alexander Middleton" => "http://www.time-meddler.co.uk",
"Sujal Shah" => "http://www.fatmixx.com",
"Peter Gasston" => "http://www.petergasston.co.uk",
"Michael Erana" => "http://g33k.efamilynj.org",
"John Hartnup" => "http://www.hartnup.net/wordpress/",
"Charles Arthur" => "http://www.charlesarthur.com/blog/",
"Paul Tomblin" => "http://xcski.com/blogs/pt/",
"Brendon Connelly" => "http://slackermanager.com/",
"Henrik" => "http://myworld.se/",
"Indiana Jones' School of Management" => "http://ijsm.org/",
"Adam Harvey" => "http://www.organicmechanic.org/",
"Alhena" => "http://www.alhena.net/blog/",
"Christa St. Jean" => "http://www.awfulsouls.com/blog/",
"Xavier Borderie" => "http://xavier.borderie.net/blog/",
"Shabby Elements" => "http://www.rigdonia.com/wp/",
"Pedro Vera-Perez" => "http://veraperez.com/",
"Joss" => "http://www.jossmer.org.uk/blog/",
"Steve Rider" => "http://steverider.org/",
"IndependentSources.com" => "http://independentsources.com",
"Andreas Schuh" => "http://www.greenlemon.org",
"Kelson Vibber" => "http://www.hyperborea.org/journal/",
"David Pinard" => "http://ncschoolmatters.com/",
"Mark Coffey" => "http://decision08.net/",
"Riley" => "http://www.rileycat.com/",
"RedNeck" => "http://www.redneckramblings.com/",
"Sebastian Herp" => "http://www.sebbi.de/",
"S Garza" => "",
"Timothy West" => "",
"Daveb" => "http://davebgimp.com/",
"Edward Mitchell" => "http://mitchellconsulting.net/commonsense/",
"Jeff Crossett" => "http://www.crossedconnections.org/w/",
"Nathan Lorenz" => "http://www.radicalwacko.com/",
"Jose Sanchez" => "http://www.sanchezconsulting.net/",
"Cricket" => "http://randomchirps.ws/",
"Chris McLaren" => "http://www.chrismclaren.com/blog/",
"Nitin Pai" => "",
"Matt" => "http://www.1115.org",
"Brian Bonner" => "http://uncooperativeblogger.com/",
"André Fiebig" => "http://www.finanso.de",
"Christopher Van Epps" => "http://www.cvanepps.com",
"Darryl Holman" => "http://hominidviews.com",
"Kev Needham" => "http://kev.needham.ca/blog/",
"J.D. Hodges" => "http://www.jdhodges.com",
"Danweasel" => "http://danweasel.com/",
"Dekay" => "http://www.dekay.org/blog",
"Joshing" => "http://joshing.org",
"Dan Tobin" => "http://www.dantobindantobin.com/blog/",
"James" => "http://mapofthedead.com/",
"John Epperson" => "",
"Lee Sonko" => "http://lee.org/blog/",
"Antiorario" => "http://orme.antiorario.it/",
"Darren John Rowse" => "http://problogger.net/",
"Jim Galley" => "http://blog.galley.net",
"Grant Cummings" => "http://www.pepperguy.com/newblog/",
"Webslum Internet Services" => "http://www.webslum.net/",
"Miraz" => "http://mactips.info/blog/index.php",
"Michael Moncur" => "http://www.figby.com/",
"Richard Silverstein" => "http://www.richardsilverstein.com/tikun_olam/",
"Pedro Timoteo" => "http://tlog.dehumanizer.com",
"Will Howells" => "http://www.willhowells.org.uk/blog/",
"Alan H Levine" => "http://dommy.com/alan/",
"Mark Nameroff" => "",
"Jessamyn West" => "http://www.librarian.net/",
"Scott Winder" => "",
"Cary Miller" => "http://www.cancer-news-watch.com",
"William Koch" => "http://youfailit.net/",
"Walter Hutchens" => "",
"War's Realm" => "",
"Oliver White" => "",
"Jacob and Claire" => "http://www.estelledesign.com/what/",
"Stephen McDonnell" => "",
"Nicholas Meyer" => "",
"Liberta" => "www.libertini.net/liberta/",
"Peter Elst" => "http://www.peterelst.com/blog/",
"Nathaniel Stern" => "http://nathanielstern.com/",
"Paolo Brocco" => "http://www.pbworks.net/",
"Mulligan" => "http://www.cantkeepquiet.com/",
"Jana" => "http://www.cloudsinmycoffee.com/",
"Joseph G." => "http://monotonous.net",
"Ari Kontiainen" => "",
"Jason Borneman" => "http://www.xtra-rant.com/",
"Hilary" => "http://www.superjux.com/",
"Christopher" => "http://www.whatintarnation.net/blog/",
"Chris Rose" => "http://www.offlineblog.com",
"Kevin" => "http://www.grinberg.ws/blog/",
"Tijl Kindt" => "http://thequicky.net",
"Alexander Trust" => "http://www.sajonara.de/",
"Tracey the Keitai Goddess" => "http://tracey.unknowgenius.com",
"Rian Stockbower" => "http://rianjs.net",
"Jewels Web Graphics" => "http://www.jewelswebgraphics.com",
"Kenneth Cooper" => "",
"Phil Smith" => "",
"Scotfl" => "http://scotfl.ca/",
"Ryan Waddell" => "http://www.ryanwaddell.com/",
"James Tippett" => "",
"Pfish" => "http://www.pfish.org",
"Matt Scoville" => "http://cupjoke.com",
"Keith Constable" => "http://kccricket.net/",
"Stephen Sherry" => "http://www.marginalwalker.co.uk",
"Terry Dillard" => "http://www.righttrack.us/about-me",
);

	ksort($testers);
	$i = count($testers);
	foreach ($testers as $name => $url)
	{
		if ($url)
			echo "<a href=\"$url\">$name</a>";
		else
			echo $name;
		$i--;
		if ($i == 1)
			echo " & ";
		elseif ($i)
			echo ", ";
	
	}
?>
</p></fieldset>

<p style="text-indent:0;"><em><small><?php _e('Did I leave you out? please do let me know... I\'ve done my best to ensure I had the whole team covered, but I might have missed a few, so don\'t hesitate to <a href="mailto:sk2@unknowngenius.com">send me a note</a>.', 'sk2'); ?></small></em></p>
</div>