#!/bin/python

import sys

if len(sys.argv) < 2:
	print "vsync counter"
	print "Usage vsync_cnt.py file.log [lno]"
	quit()

frame_lno = -1

if len(sys.argv) > 2:
	frame_lno = int(sys.argv[2])

f = open(sys.argv[1])

list = f.readlines()
f.close()

count = 0
lno = 0

for line in list:
	lno += 1
	if frame_lno == lno:
		print "%d line is %dsec %dframe" % (frame_lno,count/60,count)

	if line.strip() == "6502VSYNC":
		count += 1
		if count % 60 == 0:
			print "%dsec : %d" % (count/60,lno)






