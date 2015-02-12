<html>
<head><title>Esp8266 N web server</title>
<link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
<div id="main">
If you see this, it means the tiny li'l website in your ESP8266 does actually work. Fyi, this page has
been loaded <b>%counter%</b> times.
<ul>
<li>If you haven't connected this device to your WLAN network now, you can <a href="/wifi">do so.</a></li>
<li>You can also control the <a href="led.tpl">LED</a>.</li>
<li>You can download the raw <a href="flash.bin">contents</a> of the SPI flash rom</li>
</ul>
</p>

<form action="/program.cgi" enctype="multipart/form-data" method="post">
<p>
Please specify a file, or a set of files:<br>
<input type="file" name="datafile" size="40">
</p>
<input type="submit" value="Send">
</form>
</p>
</div>
</body></html>
