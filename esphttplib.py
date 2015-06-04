#!/usr/bin/env python

import socket
import sys
import time
import select
import json
import traceback


class ESP_AT:

	def __init__(self, host, port=23):
		self.host = host
		self.port = port
		self.s = None

	def __del__(self):
		self.close()

	def open(self):
		for res in socket.getaddrinfo(HOST, PORT, socket.AF_UNSPEC, socket.SOCK_STREAM):
			af, socktype, proto, canonname, sa = res
			try:
				self.s = socket.socket(af, socktype, proto)
			except socket.error as msg:
				self.s = None
				continue
			try:
				self.s.connect(sa)
			except socket.error as msg:
				self.s.close()
				self.s = None
				continue
			break
		if self.s is None:
			raise Exception('could not open socket')
		self.s.setblocking(1)
		self.s.settimeout(1.5)

	def close(self):
		self.s.close()

	def read_byte(self):
		b = bytearray(1)
		self.s.recv_into(b)
#		file('received.log','a').write(b)
		return b

	def read_dat(self, nlines, datawidth):
		rs = ''
		for i in range(nlines):
			self._read_until('DAT: ')
			l = self.read_size(datawidth)
	#		print len(l), i
			rs += l
		return rs

	def _read_until(self, s='\r'):
		rs = ''
		while True:
			rs += self.read_byte()
			if rs.endswith(s):
				break;
		return rs

	def read_size(self, size = 1):
		i = 0
		rs = ''
		while True:
			rs += self.read_byte()
			i += 1
			if i >= size:
				break
		return rs

	def read_until(self, c='\r'):
		rs = self._read_until(c)
		return rs.replace('\r', '\n').replace('\n\n', '\n')

	def send(self, request, c='\r'):
		self.s.sendall(request)
		return self.read_until(c)

	def send_at(self, cmd):
		rs = str(self.send('+++AT' + cmd + '\r')).strip()
		if rs.endswith('ERROR'):
			raise Exception('Error:' + '\n'.join(rs))
		return rs

	def send_cmu(self, cmd=''):
		rs = str(self.send(cmd + '\r')).strip()
		if cmd:
			if not (rs.endswith('ACK') or rs.endswith(':')):
				if rs.endswith('NCK'):
					raise Exception('Invalid command or parameters  %s' % cmd) 
				else:
					raise Exception('Error:' + '\n'.join(rs))
		return rs

	def send_cmu_stop(self):
		self.s.sendall('\r')
		self.flush()

	def flush(self):
		try:
			while True:
				b = self.s.recv(1)
				print >>file('garbage.log', 'a'), b
		except socket.timeout:
			pass


class ESP_CMU(ESP_AT):

	def parseData(self, data):
		rs = None
		data = str(data).lstrip()
		if data[1] == ' ':
			typ = data[0]
			data = data[2:]
			if typ == 'F':
				rs = data[:600]
			elif typ == 'H':
				rs = [int(v)*100/255 for v in data.strip().split(' ')]
			elif typ == 'S':
				rs = dict(zip(('rmean', 'gmean', 'bmean', 'rmedian','gmedian','bmedian', 'rmode', 'gmode', 'bmode', 'rstdev', 'gstdev', 'bstdev', ), [float(v) for v in data.strip().split(' ')]))
			elif typ == 'T':
				rs = dict(zip(('mx', 'my', 'x1', 'y1', 'x2', 'y2', 'pixels', 'confidence'), [float(v) for v in data.strip().split(' ')]))
		if rs is None:
			raise Exception('Not parseable data (size: %d, type:%s, space:0x%2.2X): %s ...' % (len(data), data[:1], ord(data[1]) if isinstance(data[1], basestring) else data[1], data[:10]))
		return rs

	def file_data_bin(self):
	#	self.send_at('LFLASH')
		self.send_cmu('SF 3 3')
		f = file('data.bin','w')
		f.write(read_dat(80, 60*2))
		self.send_cmu_stop()

	def file_bwdata_bin(self):
		self.send_cmu('SB')
		f = file('bwdata.bin','w')
		f.write(read_size(600))
		self.send_cmu_stop()

	def file_gh_html(self):
		self.send_cmu('CT 0')

		self.send_cmu('GH 0 5')
		hred = self.parseData(read_until())
		self.send_cmu_stop()
		self.send_cmu('GH 1 5')
		hgreen = self.parseData(read_until())
		self.send_cmu_stop()
		self.send_cmu('GH 2 5')
		hblue = self.parseData(read_until())
		self.send_cmu_stop()

		self.send_cmu('CT 1')

		self.send_cmu('GH 0 5')
		hV = self.parseData(read_until())
		self.send_cmu_stop()
		self.send_cmu('GH 1 5')
		hY = self.parseData(read_until())
		self.send_cmu_stop()
		self.send_cmu('GH 2 5')
		hU = self.parseData(read_until())
		self.send_cmu_stop()

		f = file('gh.html','w')

		binsize = 256/len(hred)

		print >>f, '<table><tr>'
		print >>f, '<td></td><td>R</td><td>G</td><td>B</td><td>V</td><td>U</td><td>Y</td></tr><tr>'
		for t,r,g,b, v, u, y in zip([('%s-%s' % (i, i+binsize-1)) for i in range(0, 256, binsize)],
				hred, hgreen, hblue, hV, hU, hY):
			print >>f, '<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>' % (t, r, g, b, v, u, y)
		print >>f, '</tr></table>'
    
		f.close()

	def show_gv(self):
		self.send_cmu('GV')
		print self.read_until('\r')

	def file_twdata(self):
		self.send_cmu('TW 5 5 5')
		f = file('twdata.json','w')
		json.dump(self.parseData(self.read_until()), f)

	def file_tcdata(self):
		self.send_cmu('TC 96 100 0 255 0 255')
	#	self.send_cmu('TC')
		f = file('tcdata.json','w')
		json.dump(self.parseData(self.read_until()), f)

	def file_mean(self):
		self.send_cmu('GM')
		f = file('mean.json','w')
		json.dump(self.parseData(self.read_until()), f)


if __name__ == '__main__':

	HOST = '192.168.1.153'	  # The remote host
	PORT = 23			  # The same port as used by the server

	def run(esp):
		esp.send_at('USEC')
#		esp.send_at('BAUD 19200')
#		esp.send_at('BAUD 38400')
		esp.send_at('BAUD 115200')
#		esp.send_cmu_stop()

		# put to deep sleep
#		esp.send_cmu('SS 0 1 1500')
#		esp.send_cmu('SS 1 1 1500')
#		time.sleep(4)
#		esp.send_cmu('SD')
		#esp.send_at('LFLASH')
		print 'End'

	file('garbage.log','w')
	file('received.log','w')

	esp = ESP_CMU(HOST, PORT)
	esp.open()
	try:
		run(esp)
	except Exception:
		traceback.print_exc(10, file('error.log', 'w'))
	finally:
		esp.close()


