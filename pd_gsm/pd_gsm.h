//
//	Object: pd_gsm.cpp
//	Version: 1.0
//	Author: Gottfried Haider
//	Last Change: 29.8.2006
//	Developed with: Microsoft Visual C++ 8.0, pd 0.39-2
//
//	Description: This file exposes libNokiaNetmon functionality as pd 
//  objects.
//

#ifndef PD_GSM_H
#define PD_GSM_H

#include "m_pd.h"								// for t_class, etc
#include "libNokiaNetmon/libNokiaNetmon.h"		// for BASE

#define EXP extern "C" __declspec (dllexport)


//	Structs


static t_class	*c_gsm;			// "class" for opening/closing a connection
typedef struct _gsm {
	t_object	x_obj;
} t_gsm;

static t_class	*c_gsm_avg;		// class for calculating a moving average
typedef struct _gsm_avg {
	t_object	x_obj;
	t_float		avg;			// current average
	t_float		chan;			// channel number
	t_float		pt;				// n-point average
} t_gsm_avg;

static t_class	*c_gsm_chan;	// class for returning the value of a given channel
typedef struct _gsm_chan {
	t_object	x_obj;
	t_float		chan;			// channel number
} t_gsm_chan;

static t_class	*c_gsm_loc;		// class for returning position information
typedef struct _gsm_loc {
	t_object	x_obj;
	t_outlet	*country_out;	// Mobile Country Code (MCC, Austria is 232)
	t_outlet	*network_out;	// Mobile Network Code (MNC, yesss! is 5 in Austria)
	t_outlet	*area_out;		// Location Area {Identifier,Code} (LAI/LAC)
	t_outlet	*cell_out;		// Cell Identifier
} t_gsm_loc;

static t_class	*c_gsm_num;		// class for returning number of channels
typedef struct _gsm_num {
	t_object	x_obj;
} t_gsm_num;

static t_class	*c_gsm_sort;	// class for returning sorted value/channel pairs
typedef struct _gsm_sort {
	t_object	x_obj;
	t_float		num;			// zero-based index
	t_float		prev_chan;		// previous channel
	t_outlet	*p_out;			// power
	t_outlet	*chan_out;		// channel number
	t_outlet	*changed_out;	// bang if channel number has changed
} t_gsm_sort;

struct NMTHREAD					// struct that is being passed to the Netmonitor thread
{
	BASE			**base;		// pointer to a BASE pointer
	HANDLE			hMutex;		// mutex protecting that pointer
	HANDLE			hSignal;	// signal handle to end this thread
	LOC				*loc;		// pointer to a LOC struct being filled
	unsigned int	port;		// COM port to be used
};


//	Exported functions


EXP void gsm_setup(void);


//	Helper functions


// gsm class
void *gsm_new(void);
void gsm_close(t_gsm *x);
void gsm_open(t_gsm *x, t_floatarg f);
// gsm_avg class
void *gsm_avg_new(void);
void gsm_avg_bang(t_gsm_avg *x);
// gsm_chan class
void *gsm_chan_new(void);
void gsm_chan_bang(t_gsm_chan *x);
// gsm_loc class
void *gsm_loc_new(void);
void gsm_loc_bang(t_gsm_loc *x);
void gsm_loc_free(t_gsm *x);
// gsm_num class
void *gsm_num_new(void);
void gsm_num_bang(t_gsm_num *x);
// gsm_sort class
void *gsm_sort_new(void);
void gsm_sort_bang(t_gsm_sort *x);
// locking
void _baseUnlock();
BASE *_getBase(void);
// netmonitor thread
bool _getNetmonState(void);
bool _startNetmonThread(unsigned int port);
DWORD WINAPI netmonThread(LPVOID lpParam);
void _stopNetmonThread(void);


#endif		// PD_GSM_H