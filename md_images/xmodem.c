/*
from http://www.menie.org/georges/embedded/
*/


//don't use table
//#include "crc16.h"

#include <rtthread.h>

#include "wmii.h"
#include "wmii_lib.h"
#include "wmii_serial.h"


#define SOH  0x01
#define STX  0x02
#define EOT  0x04
#define ACK  0x06
#define NAK  0x15
#define CAN  0x18
#define CTRLZ 0x1A

#define MAXRETRANS 25

extern rt_device_t shell_io;
extern struct rt_semaphore shell_io_sem;
extern int shell_io_sem_wait;
#define XMODEM_BULK_TRANSFER


unsigned short crc16_ccitt( const char *buf, int len )
{
	unsigned short crc = 0;
	while( len-- ) {
		int i;
		crc ^= *buf++ << 8;
		
		for( i = 0; i < 8; ++i ) {
			if( crc & 0x8000 )
				crc = (crc << 1) ^ 0x1021;
			else
				crc = crc << 1;
		}
	}
	return crc;
}

 int _inbyte(unsigned short sec) // sec
{

	rt_err_t rx_result;
	int ticks = sec * RT_TICK_PER_SECOND;
	char ch;
	rx_result = rt_device_read(shell_io, 0, &ch, 1);

	while(rx_result != 1 && ticks > 0)
	{
		rt_thread_delay(1);
		ticks--;
		rx_result = rt_device_read(shell_io, 0, &ch, 1);
	}

	if(rx_result != 1)
		return -2;

	return 0xFF&ch;
}



void _outbyte(char c)
{
	rt_device_write(shell_io, 0, (void*)&c , 1);
}

#ifdef XMODEM_BULK_TRANSFER

int _innbyte(char *ch, int n, unsigned short sec) // sec
{

	int ticks = sec * RT_TICK_PER_SECOND;

	int pass = 0;
	
	pass += rt_device_read(shell_io, 0, ch + pass, n - pass);

	while(pass != n && ticks > 0)
	{
		rt_thread_delay(1);
		ticks--;
		pass += rt_device_read(shell_io, 0, ch + pass, n - pass);
	}

	return pass;
}

void _outnbyte(char *c, int n)
{
	int pass = 0;
	//write may only accept part of n
	pass += rt_device_write(shell_io, 0, c + pass, n-pass);
	while(pass < n){
		rt_thread_delay(1);
		pass += rt_device_write(shell_io, 0, c + pass, n-pass);
	}
}
#endif


static int check(int crc, const char *buf, int sz)
{
	if (crc) {
		unsigned short crc = crc16_ccitt(buf, sz);
		unsigned short tcrc = (buf[sz]<<8)+buf[sz+1];
		if (crc == tcrc)
			return 1;
	}
	else {
		int i;
		unsigned char cks = 0;
		for (i = 0; i < sz; ++i) {
			cks += buf[i];
		}
		if (cks == buf[sz])
		return 1;
	}

	return 0;
}

static void flushinput(void)
{
	while (_inbyte(1) >= 0);
}

int sr(int destsz, int (* handle)(void* ctx, char *p, int len), void *ctx)
{
	int ret = 0, i, wait_old;
	int glb_cnt = 0;
	char *xbuff, xor;

	#define SR_BLOCK_SIZE (512+1)
	#define SR_TIMEOUT 5


	xbuff = os_malloc(SR_BLOCK_SIZE);
	if(!xbuff)return ret;


	rt_sem_take(&shell_io_sem, -1);
	wait_old = shell_io_sem_wait;
	shell_io_sem_wait = 0;	//prevent other thread output


	while(glb_cnt < destsz){

		if(_innbyte(xbuff, SR_BLOCK_SIZE, SR_TIMEOUT) !=  SR_BLOCK_SIZE){
			ret = -1;
			goto SafeExit;
		}

		//校验
		xor = 0;
		for(i = 0; i < SR_BLOCK_SIZE; i++)
			xor ^= xbuff[i];

		//写入
		if(xor == 0){
			(*handle)(ctx, xbuff, SR_BLOCK_SIZE-1);
			glb_cnt += SR_BLOCK_SIZE -1;
			_outbyte(ACK);
		}
		else
			_outbyte(NAK);


	}

SafeExit:
	os_free(xbuff);
	shell_io_sem_wait = wait_old;	//restore wait time
	rt_sem_release(&shell_io_sem);
	return ret;


}

int NOTRecvCB(int max, int (* handle)(void* ctx, char *p, int len), void *ctx)
{
	char *xbuff;
	int cnt = 0, wait_old, ret = 0;

	xbuff = os_malloc(EXCH_BLOCK_SIZE+16); /* 1024  + 1 sumup */
	if(!xbuff){
		ret = OS_NOMEM;
		goto MemErrExit;
	}

	rt_sem_take(&shell_io_sem, -1);
	wait_old = shell_io_sem_wait;
	shell_io_sem_wait = 0;	//prevent other thread output

	while(1){
		int tried = 0;
		int recv = 0;
retry:
//		recv = _innbyte(xbuff, EXCH_BLOCK_SIZE+1, EXCH_SLAVE_TIMEOUT);
		recv = serial_read_wait(shell_io, xbuff, EXCH_BLOCK_SIZE+1, EXCH_SLAVE_TIMEOUT * RT_TICK_PER_SECOND);

		if(recv == 0)	//stop supply
			break;

		else if(recv < EXCH_BLOCK_SIZE+1){		//lost data
//			_outbyte('N');
			serial_write(shell_io, "N" , 1);
			if(tried++ > 3 ){ret = OS_TRYOUT; break;}
			goto retry;
		}

		else if(os_sum_up((unsigned char*)xbuff, EXCH_BLOCK_SIZE+1)){ //data corrupt
//			_outbyte('N');
			serial_write(shell_io, "N" , 1);
			if(tried++ > 3 ){ret = OS_TRYOUT; break;}
			goto retry;
		}
		else{		//data OK
			if(cnt >= max){//stop recv, make master timout
				rt_thread_delay(EXCH_MASTER_TIMEOUT * RT_TICK_PER_SECOND); 
				break;
			}else{
	//			_outbyte('O');
				(* handle)(ctx, xbuff, EXCH_BLOCK_SIZE);
				serial_write(shell_io, "O" , 1);
				cnt += EXCH_BLOCK_SIZE;
			}
		}
	}

//SafeExit:
	shell_io_sem_wait = wait_old;	//restore wait time
	rt_sem_release(&shell_io_sem);

MemErrExit:
	if(xbuff)os_free(xbuff);
	return ret;
}


int xmodemRecvCB(int destsz, int (* handle)(void* ctx, char *p, int len), void *ctx)
{
	char *xbuff;
	char *p;
	int wait_old, bufsz, crc = 0;
	char trychar = 'C';
	unsigned char packetno = 1;
	int ret = 0, c, len = 0;
	int retry, retrans = MAXRETRANS;

	xbuff = os_malloc(1032); /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	if(!xbuff)return ret;

	rt_sem_take(&shell_io_sem, -1);
	wait_old = shell_io_sem_wait;
	shell_io_sem_wait = 0;	//prevent other thread output
	for(;;) {
		for( retry = 0; retry < 16; ++retry) {
			if (trychar) _outbyte(trychar);
			if ((c = _inbyte(2)) >= 0) {
				switch (c) {
				case SOH:
					bufsz = 128;
					goto start_recv;
				case STX:
					bufsz = 1024;
					goto start_recv;
				case EOT:
					flushinput();
					_outbyte(ACK);
					ret =  len; /* normal end */
					goto SafeExit;
				case CAN:
					if ((c = _inbyte(1)) == CAN) {
						flushinput();
						_outbyte(ACK);
						ret = -1;
						goto SafeExit; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}

		
		if (trychar == 'C') { trychar = NAK; continue; }
		flushinput();
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		ret = -2;
		goto SafeExit;		
		/* sync error */



start_recv:
		if (trychar == 'C') crc = 1;
		trychar = 0;
		p = xbuff;
		*p++ = c;


		#ifdef XMODEM_BULK_TRANSFER
			if ((c = _innbyte(p, bufsz+(crc?1:0)+3, 2)) != bufsz+(crc?1:0)+3) goto reject;
			p += bufsz+(crc?1:0)+3;
		#else
		for (i = 0;  i < (bufsz+(crc?1:0)+3); ++i) {
			if ((c = _inbyte(1)) < 0) goto reject;
			*p++ = c;
		}
		#endif
		
		if (xbuff[1] == (unsigned char)(~xbuff[2]) && 
			(xbuff[1] == packetno || xbuff[1] == (unsigned char)packetno-1) &&
			check(crc, &xbuff[3], bufsz)) {
			if (xbuff[1] == packetno)	{
				int count = destsz - len;
				if (count > bufsz) count = bufsz;
				if (count > 0) {
					//rt_memcpy (&dest[len], &xbuff[3], count);
					(* handle)(ctx, &xbuff[3], count);

					len += count;
				}
				++packetno;
				retrans = MAXRETRANS+1;
			}

			if (--retrans <= 0) {
				flushinput();
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				ret = -3;
				goto SafeExit;
				/* too many retry error */
			}
			_outbyte(ACK);
			continue;
		}
		
	reject:
		flushinput();
		_outbyte(NAK);
		
	}

SafeExit:
	os_free(xbuff);
	shell_io_sem_wait = wait_old;	//restore wait time
	rt_sem_release(&shell_io_sem);
	return ret;
}


int xmodemReceive(char *dest, int destsz)
{
	char *xbuff;
	char *p;
	int wait_old,bufsz, crc = 0;
	char trychar = 'C';
	unsigned char packetno = 1;
	int ret = 0, c, len = 0;
	int retry, retrans = MAXRETRANS;

	xbuff = os_malloc(1032); /* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	if(!xbuff)return ret;

	rt_sem_take(&shell_io_sem, -1);
	wait_old = shell_io_sem_wait;
	shell_io_sem_wait = 0;	//prevent other thread output

	for(;;) {
		for( retry = 0; retry < 16; ++retry) {
			if (trychar) _outbyte(trychar);
			if ((c = _inbyte(2)) >= 0) {
				switch (c) {
				case SOH:
					bufsz = 128;
					goto start_recv;
				case STX:
					bufsz = 1024;
					goto start_recv;
				case EOT:
					flushinput();
					_outbyte(ACK);
					ret =  len; /* normal end */
					goto SafeExit;
				case CAN:
					if ((c = _inbyte(1)) == CAN) {
						flushinput();
						_outbyte(ACK);
						ret = -1;
						goto SafeExit; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}

		
		if (trychar == 'C') { trychar = NAK; continue; }
		flushinput();
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		ret = -2;
		goto SafeExit;		
		/* sync error */



	start_recv:
		if (trychar == 'C') crc = 1;
		trychar = 0;
		p = xbuff;
		*p++ = c;

		#ifdef XMODEM_BULK_TRANSFER
			if ((c = _innbyte(p, bufsz+(crc?1:0)+3, 2)) != bufsz+(crc?1:0)+3) goto reject;
			p += bufsz+(crc?1:0)+3;
		#else
		for (i = 0;  i < (bufsz+(crc?1:0)+3); ++i) {
			if ((c = _inbyte(1)) < 0) goto reject;
			*p++ = c;
		}
		#endif

		if (xbuff[1] == (unsigned char)(~xbuff[2]) && 
			(xbuff[1] == packetno || xbuff[1] == (unsigned char)packetno-1) &&
			check(crc, &xbuff[3], bufsz)) {
			if (xbuff[1] == packetno)	{
				int count = destsz - len;
				if (count > bufsz) count = bufsz;
				if (count > 0) {
					rt_memcpy (&dest[len], &xbuff[3], count);
					len += count;
				}
				++packetno;
				retrans = MAXRETRANS+1;
			}

			if (--retrans <= 0) {
				flushinput();
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				ret = -3;
				goto SafeExit;
				/* too many retry error */
			}
			_outbyte(ACK);
			continue;
		}
		
	reject:
		flushinput();
		_outbyte(NAK);
		
	}

SafeExit:
	os_free(xbuff);
	shell_io_sem_wait = wait_old;	//restore wait time
	rt_sem_release(&shell_io_sem);
	return ret;

}

//返回长度
int xmodemTransCB(int (* handle)(void* ctx, char *p, int len), void *ctx)
{
	int srcsz = 0;

	char *xbuff; 
	int wait_old, bufsz, crc = -1;
	unsigned char packetno = 1;
	int ret = 0, i, c, len = 0;
	int retry;

	xbuff = os_malloc(1032);/* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	if(!xbuff)return OS_NOMEM;

	rt_sem_take(&shell_io_sem, -1);
	wait_old = shell_io_sem_wait;
	shell_io_sem_wait = 0;	//prevent other thread output

	for(;;) {
		for( retry = 0; retry < 16; ++retry) {
			if ((c = _inbyte(1)) >= 0) {
				switch (c) {
				case 'C':
					crc = 1;
					goto start_trans;
				case NAK:
					crc = 0;
					goto start_trans;
				case CAN:
					if ((c = _inbyte(1)) == CAN) {
						_outbyte(ACK);
						flushinput();
						ret = -1;
						goto SafeExit; /* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		flushinput();
		ret = OS_TRYOUT;
		goto SafeExit;
		/* no sync */

		for(;;) {
		start_trans:
			xbuff[0] = SOH; bufsz = 128;
			xbuff[1] = packetno;
			xbuff[2] = ~packetno;

			rt_memset (&xbuff[3], 0, bufsz);

			c = (* handle)(ctx, &xbuff[3], bufsz);
			srcsz += c;

			c = srcsz - len;
			if (c > bufsz) c = bufsz;

			if (c >= 0) {
/*				rt_memset (&xbuff[3], 0, bufsz);
*/				
				if (c == 0) {
					xbuff[3] = CTRLZ;
				}else {
/*					rt_memcpy (&xbuff[3], &src[len], c);
*/
					if (c < bufsz) xbuff[3+c] = CTRLZ;
				}
				if (crc) {
					unsigned short ccrc = crc16_ccitt(&xbuff[3], bufsz);
					xbuff[bufsz+3] = (ccrc>>8) & 0xFF;
					xbuff[bufsz+4] = ccrc & 0xFF;
				}
				else {
					unsigned char ccks = 0;
					for (i = 3; i < bufsz+3; ++i) {
						ccks += xbuff[i];
					}
					xbuff[bufsz+3] = ccks;
				}
				
				for (retry = 0; retry < MAXRETRANS; ++retry) {
					
				#ifdef XMODEM_BULK_TRANSFER
					_outnbyte(xbuff, bufsz+4+(crc?1:0));
				#else
					for (i = 0; i < bufsz+4+(crc?1:0); ++i) {
						_outbyte(xbuff[i]);
					}
				#endif

					if ((c = _inbyte(1)) >= 0 ) {
						switch (c) {
						case ACK:
							++packetno;
							len += bufsz;
							goto start_trans;
						case CAN:
							if ((c = _inbyte(1)) == CAN) {
								_outbyte(ACK);
								flushinput();
								ret = OS_ERROR;
								goto SafeExit;
								/* canceled by remote */
							}
							break;
						case NAK:
						default:
							break;
						}
					}
				}
				
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				flushinput();
				ret = OS_TRYOUT;
				goto SafeExit;	/* xmit error */
			}

			
			else {
				for (retry = 0; retry < 10; ++retry) {
					_outbyte(EOT);
					if ((c = _inbyte(2)) == ACK) break;
				}
				flushinput();
				ret = (c == ACK)?len:OS_ERROR;
				goto SafeExit;
			}
		}
	}


SafeExit:
	os_free(xbuff);
	shell_io_sem_wait = wait_old;	//restore wait time
	rt_sem_release(&shell_io_sem);
	return ret;

}


int xmodemTransmit(char *src, int srcsz)
{
	char *xbuff; 
	int wait_old, bufsz, crc = -1;
	unsigned char packetno = 1;
	int ret = 0, i, c, len = 0;
	int retry;

	xbuff = os_malloc(1032);/* 1024 for XModem 1k + 3 head chars + 2 crc + nul */
	if(!xbuff)return ret;

	rt_sem_take(&shell_io_sem, -1);
	wait_old = shell_io_sem_wait;
	shell_io_sem_wait = 0;	//prevent other thread output

	for(;;) {
		for( retry = 0; retry < 16; ++retry) {
			if ((c = _inbyte(1)) >= 0) {
				switch (c) {
				case 'C':
					crc = 1;
					goto start_trans;
				case NAK:
					crc = 0;
					goto start_trans;
				case CAN:
					if ((c = _inbyte(1)) == CAN) {
						_outbyte(ACK);
						flushinput();
						ret = -1;
						goto SafeExit;
						/* canceled by remote */
					}
					break;
				default:
					break;
				}
			}
		}
		_outbyte(CAN);
		_outbyte(CAN);
		_outbyte(CAN);
		flushinput();
		ret = -2;
		goto SafeExit;
		/* no sync */

		for(;;) {
		start_trans:
			xbuff[0] = SOH; bufsz = 128;
			xbuff[1] = packetno;
			xbuff[2] = ~packetno;

			rt_memset (&xbuff[3], 0, bufsz);


			c = srcsz - len;
			if (c > bufsz) c = bufsz;


			if (c >= 0) {
/*				rt_memset (&xbuff[3], 0, bufsz);
*/				
				if (c == 0) {
					xbuff[3] = CTRLZ;
				}else {
					rt_memcpy (&xbuff[3], &src[len], c);

					if (c < bufsz) xbuff[3+c] = CTRLZ;
				}
				if (crc) {
					unsigned short ccrc = crc16_ccitt(&xbuff[3], bufsz);
					xbuff[bufsz+3] = (ccrc>>8) & 0xFF;
					xbuff[bufsz+4] = ccrc & 0xFF;
				}
				else {
					unsigned char ccks = 0;
					for (i = 3; i < bufsz+3; ++i) {
						ccks += xbuff[i];
					}
					xbuff[bufsz+3] = ccks;
				}
				
				for (retry = 0; retry < MAXRETRANS; ++retry) {
					

				#ifdef XMODEM_BULK_TRANSFER
					_outnbyte(xbuff, bufsz+4+(crc?1:0));
				#else
					for (i = 0; i < bufsz+4+(crc?1:0); ++i) {
						_outbyte(xbuff[i]);
					}
				#endif

					if ((c = _inbyte(1)) >= 0 ) {
						switch (c) {
						case ACK:
							++packetno;
							len += bufsz;
							goto start_trans;
						case CAN:
							if ((c = _inbyte(1)) == CAN) {
								_outbyte(ACK);
								flushinput();
								ret = -1;
								goto SafeExit;
								/* canceled by remote */
							}
							break;
						case NAK:
						default:
							break;
						}
					}
				}
				
				_outbyte(CAN);
				_outbyte(CAN);
				_outbyte(CAN);
				flushinput();
				ret = -4;
				goto SafeExit;
				/* xmit error */
			}

			
			else {
				for (retry = 0; retry < 10; ++retry) {
					_outbyte(EOT);
					if ((c = _inbyte(2)) == ACK) break;
				}
				flushinput();
				ret = (c == ACK)?len:-5;
				goto SafeExit;
			}
		}
	}


SafeExit:
	os_free(xbuff);
	shell_io_sem_wait = wait_old;	//restore wait time
	rt_sem_release(&shell_io_sem);
	return ret;

}




#ifdef TEST_XMODEM_RECEIVE
int main(void)
{
	int st;

	printf ("Send data using the xmodem protocol from your terminal emulator now...\n");
	/* the following should be changed for your environment:
	   0x30000 is the download address,
	   65536 is the maximum size to be written at this address
	 */
	st = xmodemReceive((char *)0x30000, 65536);
	if (st < 0) {
		printf ("Xmodem receive error: status: %d\n", st);
	}
	else  {
		printf ("Xmodem successfully received %d bytes\n", st);
	}

	return 0;
}
#endif
#ifdef TEST_XMODEM_SEND
int main(void)
{
	int st;

	printf ("Prepare your terminal emulator to receive data now...\n");
	/* the following should be changed for your environment:
	   0x30000 is the download address,
	   12000 is the maximum size to be send from this address
	 */
	st = xmodemTransmit((char *)0x30000, 12000);
	if (st < 0) {
		printf ("Xmodem transmit error: status: %d\n", st);
	}
	else  {
		printf ("Xmodem successfully transmitted %d bytes\n", st);
	}

	return 0;
}
#endif










