init = function() {
	var data = null;
	var bwdata = null;
	var twdata = null;
	var tcdata = null;

	var xoff = 10;
	var yoff = 10;

	var draw = function()
	{
		$('#canvas').each(function(idx, el) {
			var ctx = el.getContext('2d');
			ctx.clearRect (0, 0, el.width, el.height);
			var w = 60;
			var h = 80;
			var imgData=ctx.createImageData(h,w);
			var imgDataMasked=ctx.createImageData(h,w);
			var coef5 = 256/(1 << 5);
			var coef6 = 256/(1 << 6);
			for(var y=0; y<h; y++) {
				for(var x=0; x<w; x++) {
					var dp = (y*w+x)*2;

					var bwp = y+x*h;
					var bwbyte = Math.floor(bwp / 8);
					var bwbit = 7 - (bwp % 8);

					var msk = (bwdata[bwbyte] >> bwbit) & 1;

					var word = data[dp] + (data[dp+1] << 8);
//					var b1 = (word & 0x1f) * coef5;
//					var g1 = ((word >> 5) & 0x3f) * coef6;
//					var r1 = (word >> 11) * coef5;
					var b = (data[dp] & 0x1f) * coef5;
					var g = ((data[dp] >> 5) + ((data[dp+1] & 0x7) << 3)) * coef6;
					var r = (data[dp+1] >> 3) * coef5;
					dp = (y+x*h)*4;
					imgData.data[dp] = r;
					imgData.data[dp+1] = g;
					imgData.data[dp+2] = b;
					imgData.data[dp+3] = 255;
					imgDataMasked.data[dp] = r;
					imgDataMasked.data[dp+1] = g;
					imgDataMasked.data[dp+2] = b;
					imgDataMasked.data[dp+3] = (msk) ? 255 : 0;
				}
			}
			ctx.putImageData(imgData, xoff, yoff);
			ctx.beginPath();
			ctx.lineWidth="1";
			ctx.strokeStyle="green";
			ctx.rect(xoff+twdata.x1/2, yoff+twdata.y1/2, (twdata.x2-twdata.x1)/2, (twdata.y2-twdata.y1)/2);
			ctx.stroke();
			ctx.beginPath();
			ctx.lineWidth="0";
			ctx.fillStyle="orange";
			ctx.fillRect(xoff+twdata.mx/2 - 2, yoff+twdata.my/2 - 2, 4, 4);
			ctx.stroke();

			ctx.putImageData(imgData, xoff + 100, yoff);
			ctx.beginPath();
			ctx.lineWidth="1";
			ctx.strokeStyle="blue";
			ctx.rect(100+xoff+tcdata.x1/2, yoff+tcdata.y1/2, (tcdata.x2-tcdata.x1)/2, (tcdata.y2-tcdata.y1)/2);
			ctx.stroke();
			ctx.beginPath();
			ctx.lineWidth="0";
			ctx.fillStyle="ping";
			ctx.fillRect(100+xoff+tcdata.mx/2 - 2, yoff+tcdata.my/2 - 2, 4, 4);
			ctx.stroke();

			// add mask
			ctx.putImageData(imgDataMasked, xoff, yoff+100);
		});
	}

	var xhr = new XMLHttpRequest();
	xhr.open('GET', 'cam/data.bin', true);
	//xhr.responseType = 'blob';
	xhr.responseType = 'arraybuffer';
	xhr.onload = function(e) {
		if (this.status == 200) {
			// get binary data as a response
			//var blob = this.response;
			//console.log(blob.size);
			data = new Uint8Array(this.response);
		} else {
			alert('error');
		}
	};
	xhr.send();
	var xhr = new XMLHttpRequest();
	xhr.open('GET', 'cam/bwdata.bin', true);
	//xhr.responseType = 'blob';
	xhr.responseType = 'arraybuffer';
	xhr.onload = function(e) {
		if (this.status == 200) {
			// get binary data as a response
			//var blob = this.response;
			//console.log(blob.size);
			bwdata = new Uint8Array(this.response);
		} else {
			alert('error');
		}
	};
	xhr.send();
	var xhr = new XMLHttpRequest();
	xhr.open('GET', 'cam/twdata.json', true);
	xhr.onload = function(e) {
		if (this.status == 200) {
			twdata = JSON.parse(this.response);
		} else {
			alert('error');
		}
	};
	xhr.send();
	var xhr = new XMLHttpRequest();
	xhr.open('GET', 'cam/tcdata.json', true);
	xhr.onload = function(e) {
		if (this.status == 200) {
			tcdata = JSON.parse(this.response);
		} else {
			alert('error');
		}
	};
	xhr.send();
	var count = 10;
	var try_draw = function() {
		if(data && bwdata && twdata && tcdata) {
			draw();
		} else {
			count--;
			if(count > 0) {
				setTimeout(try_draw, 1000);
			} else {
				alert('Data can not be loaded');
			}
		}
	};
	setTimeout(try_draw, 1000);
};
