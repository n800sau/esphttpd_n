#!/usr/bin/env python

import socket
import sys
import time
import select

HOST = '192.168.1.97'	  # The remote host
PORT = 23			  # The same port as used by the server

file('garbage.log','w')
file('received.log','w')

s = None
for res in socket.getaddrinfo(HOST, PORT, socket.AF_UNSPEC, socket.SOCK_STREAM):
	af, socktype, proto, canonname, sa = res
	try:
		s = socket.socket(af, socktype, proto)
	except socket.error as msg:
		s = None
		continue
	try:
		s.connect(sa)
	except socket.error as msg:
		s.close()
		s = None
		continue
	break
if s is None:
	print 'could not open socket'
	sys.exit(1)

s.setblocking(1)
s.settimeout(1)

def read_byte():
	b = bytearray(1)
	s.recv_into(b)
	file('received.log','a').write(b)
	return b

def read_dat(nlines, datawidth):
	rs = ''
	for i in range(nlines):
		_read_until('DAT: ')
		l = read_size(datawidth)
#		print len(l), i
		rs += l
	return rs

def _read_until(s='\r'):
	rs = ''
	while True:
		rs += read_byte()
		if rs.endswith(s):
			break;
	return rs

def read_size(size = 1):
	i = 0
	rs = ''
	while True:
		rs += read_byte()
		i += 1
		if i >= size:
			break
	return rs

def read_until(c='\r'):
	rs = _read_until(c)
	return rs.replace('\r', '\n').replace('\n\n', '\n')

def send(request, c='\r'):
	s.sendall(request)
	return read_until(c)

def send_at(cmd):
	rs = str(send('+++AT' + cmd + '\r')).strip()
	if rs.endswith('ERROR'):
		raise Exception('Error:' + '\n'.join(rs))
	return rs

def send_cmu(cmd=''):
	rs = str(send(cmd + '\r')).strip()
	if cmd:
		if not (rs.endswith('ACK') or rs.endswith(':')):
			if rs.endswith('NCK'):
				raise Exception('Invalid command or parameters  %s' % cmd) 
			else:
				raise Exception('Error:' + '\n'.join(rs))
	return rs

def send_cmu_stop():
	s.sendall('\r')
	flush()

def flush():
	try:
		while True:
			b = s.recv(1)
			print >>file('garbage.log', 'a'), b
	except socket.timeout:
		pass

def parseData(data):
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
			rs = [float(v) for v in data.strip().split(' ')]
		elif typ == 'T':
			rs = dict(zip(['mx', 'my', 'x1', 'y1', 'x2', 'y2', 'pixels', 'confidence'], [float(v) for v in data.strip().split(' ')]))
	if rs is None:
		raise Exception('Not parseable data (size: %d, type:%s, space:0x%2.2X): %s ...' % (len(data), data[:1], data[1], data[:10]))
	return rs

def run():
	send_at('USEC')
	send_at('BAUD 19200')
	send_cmu_stop()
	#send_cmu('GV')
	#print read_until('\r')
	# switch to color track mode (opposite to yuv)
	send_cmu('CT 0')
	send_at('LFLASH')
	send_cmu('SF 3 3')
	f = file('data.bin','w')
	f.write(read_dat(80, 60*2))
	send_cmu_stop()
#	send_cmu('CT 0')
#	send_cmu('ST')
	send_cmu('SB')
	f = file('bwdata.bin','w')
	f.write(read_size(600))
	send_cmu_stop()
	send_cmu('GH 0 5')
	hred = parseData(read_until())
	send_cmu_stop()
	send_cmu('GH 1 5')
	hgreen = parseData(read_until())
	send_cmu_stop()
	send_cmu('GH 2 5')
	hblue = parseData(read_until())
	send_cmu_stop()

	f = file('gh.html','w')

	binsize = 256/len(hred)

	print >>f, '<table><tr>'
	for t,r,g,b in zip([('%s-%s' % (i, i+binsize-1)) for i in range(0, 256, binsize)], hred, hgreen, hblue):
		print >>f, '<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>' % (t, r, g, b)
	print >>f, '</tr></table>'

	f.close()

	send_cmu('ST 16 31 16 31 16 31')

	print 'End'

try:
	run()
except Exception, e:
	print >>file('error.log', 'w'), e

s.close()
