<html>
<head><title>Esp8266 N web server</title>

<link rel="stylesheet" type="text/css" href="style.css">

</head>
<body>
<div id="main">
ESP8266 esphttpd_n site. This page has been loaded <b>%counter%</b> times.
<ul>
<li>If you haven't connected this device to your WLAN network now, you can <a href="/wifi">do so.</a></li>
<li>You can also control the <a href="led.tpl">LED</a>.</li>
<li>You can download the raw <a href="flash.bin">contents</a> of the SPI flash rom</li>
</ul>
</p>

<h2>You can upload a hex file to flash connected Arduino</h2>
<form action="/program.cgi" enctype="multipart/form-data" method="post">
Please choose serial port baud rate:
<select name="baud">
<option value="19200">19200 (Pro 168, Mini 168)</option>
<option value="38400">38400</option>
<option value="57600">57600 (Pro 328)</option>
<option value="115200" selected="selected">115200 (UNO, Mini 328)</option>
</select>
<p>
Please choose a file:<br>
<input type="file" name="datafile" size="40">
</p>
<input type="submit" value="Send">
</form>
</p>
</div>

</body>
</html>
