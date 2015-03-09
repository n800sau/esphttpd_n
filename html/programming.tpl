<html>
<head>
<script>
	if(%is_return%) {
		setTimeout('window.location.href = "/"', 5000);
	} else {
		if(! %is_error%) {
			setTimeout('window.location.reload()', 5000);
		}
	}
</script>
</head>
<body>

Programming status: %prog_status%<br/>
%status_msg%<br/>
Bootloader: %bl_version%<br/>
Signature: %signature%<br/>

<a href="/">Home</a>

</body>
</html>
