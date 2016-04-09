#coding=utf-8
import ctypes,logging,datetime

LOGNAME = "miio"

def dolog(msg):
    log_file = datetime.datetime.now().strftime( LOGNAME + ".%Y-%m-%d.log")
    logging.basicConfig(format='%(asctime)s|%(message)s', filename= log_file, level=logging.INFO , datefmt="%Y-%m-%d %H:%M:%S")
    logging.info(msg)
    pass

def logPass(mac=""):
    dolog("%s|PASS|0" % (mac))
    clr = Color()
    clr.print_green_text("""\n==================\n=      PASS      =\n==================\n""")

def logOK(mac=""):
    dolog("%s|OK|0" % (mac))
    clr = Color()
    clr.print_blue_text("""\n==================\n=       OK       =\n=  PRESS  RESET  =\n==================\n""")


def logFail(mac="",code=-1):
    dolog("%s|FAIL|%s" % (mac,code))
    clr = Color()
    clr.print_red_text("""\n==================\n=      FAIL      =\n==================\n""")

def logWarning(mac="",code=-1):
    dolog("%s|WARN|%s" % (mac,code))

def logDidmac(did="",mac=""):
    dolog("%s|CHECKPASS|0|%s" % (mac.upper(),did))
    clr = Color()
    clr.print_green_text("""DID:%s\nMAC:%s\n""" % (did,mac.upper()) )


STD_INPUT_HANDLE = -10
STD_OUTPUT_HANDLE= -11
STD_ERROR_HANDLE = -12

FOREGROUND_BLUE = 0x01 # text color contains blue.
FOREGROUND_GREEN= 0x02 # text color contains green.
FOREGROUND_RED = 0x04 # text color contains red.
FOREGROUND_INTENSITY = 0x08 # text color is intensified.

BACKGROUND_BLUE = 0x10 # background color contains blue.
BACKGROUND_GREEN= 0x20 # background color contains green.
BACKGROUND_RED = 0x40 # background color contains red.
BACKGROUND_INTENSITY = 0x80 # background color is intensified.


def prRed(prt): print("\033[91m {}\033[00m" .format(prt))
def prGreen(prt): print("\033[92m {}\033[00m" .format(prt))
def prYellow(prt): print("\033[93m {}\033[00m" .format(prt))
def prLightPurple(prt): print("\033[94m {}\033[00m" .format(prt))
def prPurple(prt): print("\033[95m {}\033[00m" .format(prt))
def prCyan(prt): print("\033[96m {}\033[00m" .format(prt))
def prLightGray(prt): print("\033[97m {}\033[00m" .format(prt))
def prBlack(prt): print("\033[98m {}\033[00m" .format(prt))


class Color:
    ''' See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winprog/winprog/windows_api_reference.asp
    for information on Windows APIs.'''
    #std_out_handle = ctypes.windll.kernel32.GetStdHandle(STD_OUTPUT_HANDLE)
    
    #def set_cmd_color(self, color, handle=std_out_handle):
    def set_cmd_color(self, color, handle=0):
        """(color) -> bit
        Example: set_cmd_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
        """
        #bool = ctypes.windll.kernel32.SetConsoleTextAttribute(handle, color)
        bool = 0 
        return bool
    
    def reset_color(self):
        self.set_cmd_color(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
    
    def print_red_text(self, print_text):
        self.set_cmd_color(FOREGROUND_RED | FOREGROUND_INTENSITY)
        #print print_text
        prRed(print_text)
        self.reset_color()
        
    def print_green_text(self, print_text):
        self.set_cmd_color(FOREGROUND_GREEN | FOREGROUND_INTENSITY)
        #print print_text
        prGreen(print_text)
        self.reset_color()
    
    def print_blue_text(self, print_text): 
        self.set_cmd_color(FOREGROUND_BLUE | FOREGROUND_INTENSITY)
        #print print_text
        prCyan(print_text)
        self.reset_color()
          
    def print_red_text_with_blue_bg(self, print_text):
        self.set_cmd_color(FOREGROUND_RED | FOREGROUND_INTENSITY| BACKGROUND_BLUE | BACKGROUND_INTENSITY)
        prLightPurple(print_text)
        #print print_text
        self.reset_color()    

if __name__ == "__main__":
    clr = Color()
    clr.print_red_text('red')
    clr.print_green_text('green')
    clr.print_blue_text('blue')
    clr.print_red_text_with_blue_bg('background')
