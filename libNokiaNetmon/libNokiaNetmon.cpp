//
//	Object: libNokiaNetmon.cpp
//	Version: 1.1
//	Author: Gottfried Haider
//	Last Change: 26.8.2006
//	Developed with: Microsoft Visual C++ 8.0, Sysinternals' Portmon
//
//	Description: This file implements the functions of the Nokia Netmonitor 
//	library (libNokiaNetmon) as specified in libNokiaNetmon.h and also three 
//	internal functions for sending and receiving frames as well as sending 
//	acknowledge packages.
//
//	Documentation used:
//	* http://www.embedtronics.com/nokia/fbus.html (Nokia F-Bus Protocol made simple)
//	* docs/develop/protocol from Gammu
//	* http://www.nobbi.com/download/nmmanual.pdf (Nokia Netmonitor Manual)
//
//	Notes:
//	* tested on Nokia 3310 with firmware 04.19 ("03-01-01")
//	* connected mobile must have Netmonitor activated (http://www.nobbi.com/monitor/ does that)
//	* you get a cell/signal level, even when mobile is turned of - scary..
//	* singlestep debugging seems to mess up synchronization in connectMobile()
//

#include <windows.h>
#include <math.h>
#include <stdio.h>
#include "libNokiaNetmon.h"


// global variables
HANDLE comHandles[128];		// stored handles for up to 128 COM ports
unsigned int seqNumber[4] = { 0x40, 0x40, 0x40, 0x40 };		// starting sequence numbers per COM port


//	Internal Functions


//	void _sendACK(unsigned int comPort, char cmd, char seq)
//	Description: sends an acknowledge frame
//	Parameters:
//		cmdPort		number of COM port
//		cmd			command (4th byte) of frame being acknowledged
//		seq			sequence number (third-to-last byte) of frame being acknowledged
//	Note: It is assumed that the COM port has already been opened by connectMobile().

void _sendACK(unsigned int comPort, char cmd, char seq)
{
	char cAck[] = { 0x1e, 0x00, 0x0c, 0x7f, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00 };
	DWORD dwBytesWritten;	// unused

	cAck[6]	= cmd;
	cAck[7]	= seq & 0x7;	// sequence number (lower three bytes of origianl frame)
	cAck[8] = cAck[0] ^ cAck[2] ^ cAck[4] ^ cAck[6];	// checksum (xor of odd/even bytes)
	cAck[9] = cAck[1] ^ cAck[3] ^ cAck[5] ^ cAck[7];	// checksum (xor of odd/even bytes)

	WriteFile(comHandles[comPort-1], &cAck, 10, &dwBytesWritten, NULL);
}


//	char* _receiveFrame(unsigned int comPort, char cmd)
//	Description: waits for the first frame of a given type and returns its
//	payload. Checksums of all incoming frames are being validated. All
//	valid frames are being acknowledged, no matter if they match the specified
//	type. Invalid frames are being ignored. If there is no matching
//	frame after TIMEOUT miliseconds, this function returns.
//	Parameters:
//		comPort		number of COM port
//		cmd			command (4th byte) of the desired frame
//	Return Value: payload of the frame (without sequence number stored in last
//	byte) as NULL-terminated string (note: has to be free()d by the caller), 
//	NULL if a timeout occured
//	Notes: It is assumed that the COM port has already been opened by connectMobile().
//	This function replaces occuring 0x00 bytes in the input buffer by 0x2e (ASCII .)
//	characters.

char* _receiveFrame(unsigned int comPort, char cmd)
{
	char *pBuffer = NULL, *pStart, *pTemp;
	char cChecksum;
	COMSTAT comstat;
	DWORD dwBytesRead;		// unused
	DWORD dwStartTime = GetTickCount();		// get miliseconds since system start for timeout
	int length;
	unsigned long lBufferSize = 0;

	do
	{
		// wait for data in input buffer (or timeout occurs)
		do
		{
			Sleep(10);
			ClearCommError(comHandles[comPort-1], NULL, &comstat);

			// return if timeout occured
			if (GetTickCount() - dwStartTime > TIMEOUT)
				return NULL;
		}
		while (comstat.cbInQue == 0);

		// enlarge buffer
		pBuffer = (char*)realloc(pBuffer, (size_t)lBufferSize+comstat.cbInQue);
		ReadFile(comHandles[comPort-1], pBuffer+lBufferSize, comstat.cbInQue, &dwBytesRead, NULL);
		lBufferSize += comstat.cbInQue;

		// parse buffer
		pStart = pBuffer;
		do
		{
			// search for frame header
			while (pStart+3 <= pBuffer+lBufferSize && !(*pStart == 0x1e && *(pStart+1) == 0x0c && *(pStart+2) == 0x00))
				pStart++;

			// check if enough bytes in buffer for minimal packet length
			if (pStart+9 > pBuffer+lBufferSize)
				break;

			// fetch length (we ignore MSB in pStart+4)
			length = *(pStart+5);

			// check if enough bytes in buffer for specified package length
			if (pStart+5+length+2 > pBuffer+lBufferSize)	// 5 is leader, 2 is checksum
				break;

			if (*(pStart+3) != 0x7f)
			{
				// not an ACK, so we're closer examining the frame

				// checksum even bytes
				cChecksum = *pStart;
				for (pTemp = pStart+2; pTemp<pStart+5+length+1; pTemp=pTemp+2)
					cChecksum ^= *pTemp;
				if (cChecksum != *pTemp)
				{
					// wrong checksum, skip this frame
					pStart++;
					continue;
				}
				// checksum odd bytes
				cChecksum = *(pStart+1);
				for (pTemp = pStart+3; pTemp<pStart+5+length+1; pTemp=pTemp+2)
					cChecksum ^= *pTemp;
				if (cChecksum != *pTemp)
				{
					pStart++;
					continue;
				}

				// valid frame, send ACK
				_sendACK(comPort, *(pStart+3), *(pStart+5+length));

				// check if requested frame
				if (*(pStart+3) == cmd)
				{
					// convert 0x00 to 0x2e ('.')
					pTemp = pStart+6;
					while (pTemp <= pStart+5+length)
					{
						if (*pTemp == '\0')
							*pTemp = '.';		// replacement character
						pTemp++;
					}

					// create return variable, free buffer
					char *result = (char*)malloc(length);	// we skip the sequence number at the end
					strncpy_s(result, length, pStart+6, length-1);
					free(pBuffer);

					return result;
				}

				// not the one we are waiting for
				// make sure we don't process the same frame twice
				if (pStart+5+length+2+1 > pBuffer+lBufferSize)
				{
					// there is something beyond this frame which we must preserve
					pStart = pStart+5+length+2+1;	// start of new frame
					strncpy_s(pBuffer, lBufferSize, pStart, lBufferSize - (pStart - pBuffer));
					// shrink buffer
					lBufferSize -= pStart - pBuffer;
					pBuffer = (char*)realloc(pBuffer, (size_t)lBufferSize);
					pStart = pBuffer;
				}
				else
				{
					// nothing beyond this frame, free buffer
					free(pBuffer);
					pBuffer = NULL;
					lBufferSize = 0;
					break;
				}
			}
			else
			{
				// move pStart, so we don't end up checking the same frame twice
				pStart++;
			}
		}
		while (true);
	}
	while (true);
}


//	void _sendFrame(unsigned int comPort, char cmd, char* args, int len)
//	Description: sends a frame from terminal to the mobile phone. Sequence
//	number and checksum is being calculated.
//	Parameters:
//		comPort		number of COM port
//		cmd			command (4th byte of frame)
//		args		arguments (9th byte of frame and following), not NULL-terminated
//		len			length of args in bytes
//	Notes: It is assumed that the COM port has already been opened by connectMobile().

void _sendFrame(unsigned int comPort, char cmd, char* args, int len)
{
	char cChecksum;
	DWORD dwBytesWritten;	// unused
	int i, payload;

	// calculate payload length
	payload = len + 4;
	if (payload/2 != (int)payload/2)
		payload++;		// add padding byte for odd number of bytes

	char *pTemp = (char*)calloc(payload+8, 1);
	pTemp[0] = 0x1e;	// FBUS frame id (cable)
	pTemp[1] = 0x00;	// destination (phone)
	pTemp[2] = 0x0c;	// sender (terminal)
	pTemp[3] = cmd;		// command
	pTemp[4] = 0x00;	// MSB of payload length (unused)
	pTemp[5] = payload;	// payload length
	pTemp[6] = 0x00;	// first bytes of payload (seem to be static)
	pTemp[7] = 0x01;
	strncpy_s(pTemp+8, payload+8, args, len);	// arguments (in payload)
	pTemp[8+len] = 0x01;			// also static?

	// add padding byte (second-to-last byte of payload), if required
	if (len+4 < payload)
		pTemp[5+payload-1] = 0x00;

	// sequence number (last byte of payload)
	pTemp[5+payload] = seqNumber[comPort-1];
	if (seqNumber[comPort-1] < 0x47)	// sequence numbers cycle from 40 through 47
	{
		seqNumber[comPort-1]++;
	}
	else
	{
		seqNumber[comPort-1] = 0x40;
	}

	// calculate checksum
	cChecksum = pTemp[0];
	for (i=2; i<payload+6; i=i+2)
	{
		cChecksum ^= pTemp[i];	// XOR of all even/odd bytes
	}
	pTemp[5+payload+1] = cChecksum;
	cChecksum = pTemp[1];
	for (i=3; i<payload+6; i=i+2)
	{
		cChecksum ^= pTemp[i];	// XOR of all even/odd bytes
	}
	pTemp[5+payload+2] = cChecksum;

	// send over the wire
	WriteFile(comHandles[comPort-1], pTemp, payload+8, &dwBytesWritten, NULL);
}


//	Exported Functions


//  ERRORS connectMobile(unsigned int comPort)
//	Description: opens COM port to FBUS enabled mobile and sends initialization string.
//	Parameters
//		comPort		number of COM port (valid: 1-4)
//	Return Value: SUCCESS (0) or an error code as specified in ERROR (libNokiaNetmon.h)

ERRORS connectMobile(unsigned int comPort)
{
	DCB dcb;
	HANDLE handle;
	char cComPort[11];
	static char init_char = 0x55;		// character used for device initialization
	unsigned int i;

	// check comPort
	if (comPort < 1 || comPort > sizeof(comHandles))
		return E_INVALIDPORT;	// error: invalid COM port
	if (comHandles[comPort-1] != 0)
		return E_ALREADYOPEN;	// error: COM port already open

	// create (magic) filename (needed for COM ports > 10)
	sprintf_s(cComPort, sizeof(cComPort), "\\\\.\\COM%u", comPort);

	// create file
	handle = CreateFile(cComPort, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (handle == INVALID_HANDLE_VALUE)
		return E_CANTOPENPORT;	// error cannot open COM port

	// probe COM state in order to fill the DCB struct
	if (!GetCommState(handle, &dcb))
		return E_GETPORTSTATE;	// error: cannot get COM port state

	dcb.DCBlength = sizeof(DCB);
	// fill Nokia 3310 specific parameters in struct
	dcb.BaudRate = CBR_115200;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	// flow control (setting fDtrControl is necessary, not sure about the others)
	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fOutxCtsFlow = 0;
	dcb.fRtsControl = DTR_CONTROL_DISABLE;
	dcb.fInX = 0;
	dcb.fOutX = 0;

	// set COM state
	if (!SetCommState(handle, &dcb))
		return E_SETPORTSTATE;	// error: cannot set COM port state

	// send init string (128 times 0x55 to synch with the UART)
	DWORD dwBytesWritten;		// unused
	for (i=0; i<128; i++)
	{
		// one source recommends sleeping for 10 miliseconds, but seems to work fine
		if (!WriteFile(handle, &init_char, 1, &dwBytesWritten, NULL))
			return E_SENDINITSTRING;
	}

	// store handle in global variable
	comHandles[comPort-1] = handle;

	return SUCCESS;
}


//	ERRORS getBasestations(unsigned int comPort, BASE *dest)
//	Description: writes the channels of all GSM base stations of an FBUS enabled
//	mobile and its signal levels to the linked list BASE. The order of the entries
//	is determined by the phone itself, so mostly by signal strength descending
//	(thus -dBm value increasing).
//	Parameters:
//		comPort		number of already opened COM port
//		dest		pointer to a BASE struct being filled (overwritten)
//	Return Value: SUCCESS (0), E_NOTCONNECTED if the COM port has not been opened yet
//	or E_NODATA if the device seems not connected
//	Notes: If communication with the device works but the device sees no single base
//	station, this function returns SUCCESS nontheless, but channel/p[ower] of dest
//	will be 0. *pNext of the last entry is NULL. The caller must not forget to free all
//	structs but dest.

ERRORS getBasestations(unsigned int comPort, BASE *dest)
{
	BASE *pCur = NULL;
	char *result;
	char cTemp[4];
	char cTeststring[] = { 0x7e, 0x00 };	// arguments for netmonitor tests
	unsigned int line, page;

	// check parameters
	if (comHandles[comPort-1] == 0)
		return E_NOTCONNECTED;	// error: COM port is not connected

	// write default values in dest
	dest->channel = 0;
	dest->p = 0;
	dest->pNext = NULL;

	// send security string
	_sendFrame(comPort, 0x40, "\x64\x01", 2);	// necessary for reading netmonitor values
	result = _receiveFrame(comPort, 0x40);		// wait for any return
	if (!result)
		return E_NODATA;		// error: nothing in input buffer, device not connected?
	free(result);

	for (page=3; page<=5; page++)
	{
		// walk netmonitor pages
		cTeststring[1] = page;
		_sendFrame(comPort, 0x40, cTeststring, 2);

		result = _receiveFrame(comPort, 0x40);
		if (!result)
			continue;	// timeout occured

		// parse string
		for (line=0; line<=2; line++)
		{
			if ((line*13)+12 > strlen(result))
				continue;	// BUG: not enough bytes for parsing returned

			strncpy_s(cTemp, sizeof(cTemp), result+(line*13)+4, 3);	// copy channel number
			cTemp[3] = '\0';
			if (atoi(cTemp) != 0)
			{
				// we have a valid channel number (ie. not xxx)
				if (!pCur)
				{
					pCur = dest;
				}
				else
				{
					pCur->pNext = (BASE*)malloc(sizeof(BASE));
					pCur = pCur->pNext;
				}

				pCur->channel = atoi(cTemp);

				// check if signal strength has two or three digits
				if (*(result+(line*13)+10) == '-')
				{
					strncpy_s(cTemp, sizeof(cTemp), result+(line*13)+11, 2);
					cTemp[2] = '\0';
				}
				else
				{
					strncpy_s(cTemp, sizeof(cTemp), result+(line*13)+10, 3);
					cTemp[3] = '\0';
				}
				pCur->p = atoi(cTemp);
			}
		}

		// free buffer
		free(result);
	}
	// set the pNext of the last element to NULL
	if (pCur)
		pCur->pNext = NULL;

	return SUCCESS;
}


//	ERRORS getLocation(unsigned int comPort, LOC *dest)
//	Description: writes information regarding the currently used GSM network
//	cell in struct LOC (see libNokiaNetmon.h)
//	Parameters:
//		comPort		number of already opened COM port
//		dest		pointer to a LOC struct being filled
//	Return Value: SUCCESS (0), E_NOTCONNECTED if the COM port has not been opened or 
//	E_NODATA if the device seems not connected

ERRORS getLocation(unsigned int comPort, LOC *dest)
{
	char *pTemp, *result;
	char cTemp[6];

	// check parameters
	if (comHandles[comPort-1] == 0)
		return E_NOTCONNECTED;	// error: COM port is not connected

	// send security string
	_sendFrame(comPort, 0x40, "\x64\x01", 2);	// necessary for reading netmonitor values
	result = _receiveFrame(comPort, 0x40);
	if (!result)
		return E_NODATA;		// error: nothing in input buffer, device not connected?
	free(result);

	// aquire netmonitor test data
	_sendFrame(comPort, 0x40, "\x7e\x0b", 2);
	result = _receiveFrame(comPort, 0x40);
	if (!result)
		return E_NODATA;		// BUG: device is connected, else would previous _receiveFrame fail
	
	if (strlen(result) < 48)
	{
		free(result);
		return E_NODATA;		// BUG: not enough bytes for parsing returned
	}

	// parse string
	strncpy_s(cTemp, sizeof(cTemp), result+7, 3);
	dest->country = atoi(cTemp);
	strncpy_s(cTemp, sizeof(cTemp), result+13, 3);
	dest->network = atoi(cTemp);

	pTemp = result+21;
	while (*pTemp == ' ')
		pTemp++;			// area is right-aligned
	strncpy_s(cTemp, sizeof(cTemp), pTemp, 6-(pTemp-result-20));
	dest->area = atoi(cTemp);

	strncpy_s(cTemp, sizeof(cTemp), result+34, 3);
	dest->channel = atoi(cTemp);
	strncpy_s(cTemp, sizeof(cTemp), result+43, 5);
	dest->cell = atoi(cTemp);

	free(result);

	return SUCCESS;
}


//	void disconnectMobile(unsigned int comPort)
//	Description: frees a COM Port
//	Parameters:
//		comPort		number of COM Port
//	Return Value: none

void disconnectMobile(unsigned int comPort)
{
	CloseHandle(comHandles[comPort-1]);
	comHandles[comPort-1] = 0;
}