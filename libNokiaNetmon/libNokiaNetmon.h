//
//	Object: libNokiaNetmon.h
//	Version: 1.1
//	Author: Gottfried Haider
//	Last Change: 26.8.2006
//	Developed with: Microsoft Visual C++ 8.0, Sysinternals' Portmon
//
//	Description: The NokiaNetmon[itor] library allows you to easily incorporate 
//	location based data, like signal strength of various neighbouring base stations 
//	and an absolute identifier of the currently used base station in your application.
//	NokiaNetmon talks to FBUS enabled mobile phones, such as the Nokia 3310 GSM 
//	phone using an RS-232 interface.
//
//	Notes: Connected mobiles must have Netmonitor activated (http://www.nobbi.com/monitor/ does that)
//

#ifndef LIBNOKIANETMON_H
#define LIBNOKIANETMON_H


//	Defines


#define TIMEOUT 2000		// interval in miliseconds we wait for RS-232 input


//	Structs


struct _BASE
{
	unsigned short	channel;	// GSM channel number
	unsigned int	p;			// signal strength in -p dBm (so less is better)
	_BASE			*pNext;		// pointer to the next base station (NULL, if last one)
};
typedef _BASE BASE;

typedef struct
{
	unsigned int	country;	// Mobile Country Code (MCC, Austria is 232)
	unsigned int	network;	// Mobile Network Code (MNC, yesss! is 5 in Austria)
	unsigned short	area;		// Location Area {Identifier,Code} (LAI/LAC)
	unsigned short	cell;		// Cell Identifier
	unsigned short	channel;	// Channel of cell
} LOC;


// this is atm compatible to the error codes of libTinyGPS
typedef enum
{
	SUCCESS,
	E_INVALIDPORT,			// invalid COM port
	E_ALREADYOPEN,			// specified COM port is already open
	E_CANTOPENPORT,			// cannot open specified COM Port
	E_GETPORTSTATE,			// cannot get communication settings of COM port
	E_SETPORTSTATE,			// cannot set communication settings of COM port
	E_SENDINITSTRING,		// cannot send init string
	E_NOTCONNECTED = 16,	// COM port has not been opened yet
	E_NODATA = 18			// nothing in input buffer, device not connected?
} ERRORS;


//	Exported Functions


//  ERRORS connectMobile(unsigned int comPort)
//	Description: opens COM port to FBUS enabled mobile and sends initialization string.
//	Parameters
//		comPort		number of COM port (valid: 1-4)
//	Return Value: SUCCESS (0) or an error code as specified in ERROR
ERRORS connectMobile(unsigned int comPort);

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
ERRORS getBasestations(unsigned int comPort, BASE *dest);

//	ERRORS getLocation(unsigned int comPort, LOC *dest)
//	Description: writes information regarding the currently used GSM network
//	cell in struct LOC
//	Parameters:
//		comPort		number of already opened COM port
//		dest		pointer to a LOC struct beeems not connected
//	Return Value: SUCCESS (0), E_NOTCONNECTED if the COM port has not been opened or 
//	E_NODATA if the device seems not connected
ERRORS getLocation(unsigned int comPort, LOC *dest);

//	Return Value: SUCCESS (0), E_NOTCONNECTED if the COM port has not been opened or 
//	E_NODATA if the device setLocation(unsigned int comPort, LOC *dest);

//	void disconnectMobile(unsigned int comPort)
//	Description: frees a COM Port
//	Parameters:
//		comPort		number of COM Port
//	Return Value: none
void disconnectMobile(unsigned int comPort);


#endif		// LIBNOKIANETMON_H