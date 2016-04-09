#coding=utf-8

import subprocess,sys,urllib2,re,serial,os,time,json
from config import SERIAL,BAUD_RATE,CHECK_VER
import mylog
from mylog import LOGNAME


LOGNAME="check_app"

SYS_ENCODE="utf-8" #sys.getfilesystemencoding()

BASE_DIR="/Users/xwx/projects/miio_open/wifi/MHCW03P/factory_client"

#BASE_DIR=os.path.dirname(os.path.abspath(__file__)) + "/"

def waitNormalBoot():
	while True:
		try:	
			sp = serial.Serial(SERIAL,int(BAUD_RATE),timeout=1)
			break;
		except:
			print gbk("串口" + SERIAL + "未准备好，请配置config.py")
			time.sleep(1)

	sdid = 0
	smac = ""
	sver = ""

	while True:
		try:
			line = sp.readline()
			if line is None or line.strip() == "":
				print gbk("请放入模组")

			if line is not None:

				if sdid != 0 and smac != "" and sver != "":
					break
				else:
					print line,
				
				m1 = re.match("^digital_did\\sis\\s(\\d+)$",line.strip())
				if m1:
					sdid = m1.group(1)
				m2 = re.match("^mac\\saddress\\sis\\s(\\w+)$",line.strip())
				if m2:
					smac = m2.group(1)
				m3 = re.match("^firmware:\\s([\\d\\.]+)$",line.strip())
				if m3:
					sver = m3.group(1)


				continue
			
			
		except KeyboardInterrupt, e:
				break


	return (sdid,smac,sver)
	
 
def isMacAddr(mac):
	return re.match("^[0-9A-F]{12}$",mac) is not None

def gbk(s):
	return s.decode("utf-8").encode(SYS_ENCODE)

def burn():

	sdid,smac,sver = waitNormalBoot()

	print gbk("\n=======================\n==mac %s \n==did  %s \n==ver %s \n=======================\n" % (smac.upper(),sdid,sver))
	
	code = -1
	if sdid != 0 and smac != "" and sver == CHECK_VER:
		code = 0

	if code == 0:
		mylog.logPass(smac)
	else:
		mylog.logFail(smac,code)

# main entry
print gbk("串口:%s \r\n波特率:%s \r\n 程序目錄:%s \r\n" % (SERIAL,BAUD_RATE,BASE_DIR))
print gbk("版本检查: %s" % (CHECK_VER) )
while True:
	burn()



