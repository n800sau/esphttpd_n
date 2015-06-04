#!/usr/bin/env python

from esphttplib import ESP_AT


if __name__ == '__main__':

	HOST = '192.168.1.153'	  # The remote host
	PORT = 23			  # The same port as used by the server

	def run(esp):
		esp.send_at('USEC')
		esp.send_at('BAUD 115200')
		print 'End'

	esp = ESP_AT(HOST, PORT)
	esp.open()
	try:
		run(esp)
	except Exception:
		traceback.print_exc(10, file('error.log', 'w'))
	finally:
		esp.close()


