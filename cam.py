#!/usr/bin/env python

import socket
import sys
import time
import select

HOST = '192.168.1.97'	  # The remote host
PORT = 23			  # The same port as used by the server
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
	while True:
		b = bytearray(1)
		n = s.recv_into(b)
		if n > 0:
			break
		else:
			select.select([s], [], [s], timeout=1)
	return b

def read_dat(nlines, datawidth):
	rs = ''
	for i in range(nlines):
		_read_until('DAT: ')
		l = _read_size(datawidth)
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

def _read_size(size = 1):
	i = 0
	rs = ''
	b = bytearray(1)
	while True:
		n = s.recv_into(b)
		if n > 0:
			rs += b
			i += 1
			if i >= size:
				break
		else:
			time.sleep(0.00001)
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
		if not rs.endswith('ACK'):
			raise Exception('Error:' + '\n'.join(rs))
	return rs

def send_cmu_end():
	send('\r')

send_at('USEC')
send_at('BAUD 19200')
send_cmu('GV')
print read_until('\r')
#		for i in range(10):
send_at('LFLASH')
send_cmu('SF 3 3')
f = file('data.bin','w')
f.write(read_dat(80, 60*2))
send_cmu()
print 'End'

s.close()
