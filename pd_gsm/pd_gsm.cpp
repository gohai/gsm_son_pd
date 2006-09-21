//
//	Object: pd_gsm.cpp
//	Version: 1.0
//	Author: Gottfried Haider
//	Last Change: 29.8.2006
//	Developed with: Microsoft Visual C++ 8.0, pd 0.39-2
//
//	Description: This file exposes libNokiaNetmon functionality as pd 
//	objects.
//
//	Documentation used: 
//	http://iem.at/pd/externals-HOWTO/HOWTO-externals-en.html
//

#include <windows.h>
// DEBUG
//#include <stdio.h>
#include "pd_gsm.h"

#define		MUTEX_TIMEOUT		100L		// time we wait for a mutex before aborting


BASE			g_baseBuf = { 0 };
BASE			*g_pBase = &g_baseBuf;
HANDLE			g_hMutex = 0;
HANDLE			g_hThread = 0;
LOC				g_locBuf = { 0 };
NMTHREAD		g_thread = { 0 };


EXP void gsm_setup(void)
{
	// add gsm "class"
	c_gsm = class_new(gensym("gsm"), (t_newmethod)gsm_new, (t_method)gsm_close, sizeof(t_gsm), CLASS_DEFAULT, A_NULL);
	class_addmethod(c_gsm, (t_method)gsm_close, gensym("close"), A_NULL);
	class_addmethod(c_gsm, (t_method)gsm_open, gensym("open"), A_DEFFLOAT, A_NULL);

	// add gsm_avg class
	c_gsm_avg = class_new(gensym("gsm_avg"), (t_newmethod)gsm_avg_new, 0, sizeof(t_gsm_avg), CLASS_DEFAULT, A_NULL);
	class_addbang(c_gsm_avg, gsm_avg_bang);

	// add gsm_chan class
	c_gsm_chan = class_new(gensym("gsm_chan"), (t_newmethod)gsm_chan_new, 0, sizeof(t_gsm_chan), CLASS_DEFAULT, A_NULL);
	class_addbang(c_gsm_chan, gsm_chan_bang);

	// add gsm_loc class
	c_gsm_loc = class_new(gensym("gsm_loc"), (t_newmethod)gsm_loc_new, 0, sizeof(t_gsm_loc), CLASS_DEFAULT, A_NULL);
	class_addbang(c_gsm_loc, gsm_loc_bang);

	// add gsm_loc class
	c_gsm_num = class_new(gensym("gsm_num"), (t_newmethod)gsm_num_new, 0, sizeof(t_gsm_num), CLASS_DEFAULT, A_NULL);
	class_addbang(c_gsm_num, gsm_num_bang);

	// add gsm_sort class
	c_gsm_sort = class_new(gensym("gsm_sort"), (t_newmethod)gsm_sort_new, 0, sizeof(t_gsm_sort), CLASS_DEFAULT, A_NULL);
	class_addbang(c_gsm_sort, gsm_sort_bang);

	// create mutex protecting g_pBase
	g_hMutex = CreateMutex(NULL, false, NULL);

	// display version info
	post("gsm: version 1.0 by gottfried haider");
}


void *gsm_new(void)
{
	t_gsm *x = (t_gsm*)pd_new(c_gsm);
	return (void*)x;
}

void gsm_close(t_gsm *x)
{
	_stopNetmonThread();
}

void gsm_open(t_gsm *x, t_floatarg f)
{
	if (!_getNetmonState())		// only accept this message when there is no thread running
	{
		if (!_startNetmonThread((unsigned int)f))
			post("gsm: could not create thread");
	}
}

void *gsm_avg_new(void)
{
	t_gsm_avg *x = (t_gsm_avg*)pd_new(c_gsm_avg);
	
	floatinlet_new(&x->x_obj, &x->chan);		// second inlet: channel number
	//x->pt = 3.0;								// (default: 3)
	floatinlet_new(&x->x_obj, &x->pt);			// third inlet: n-point average
	outlet_new(&x->x_obj, gensym("float"));		// outlet: average power

	return (void*)x;
}

void gsm_avg_bang(t_gsm_avg *x)
{
	BASE			*base;
	float			p = 0.0;
	unsigned int	chan = (unsigned int)x->chan;

	// search for channel
	base = _getBase();		// lock
	while (base)
	{
		if (base->channel == chan)
		{
			p = (float)base->p;
			break;
		}
		base = base->pNext;
	}
	_baseUnlock();			// unlock

	if (x->pt == 0.0)
		x->avg = 0.0;
	else if (x->avg == 0.0)
		x->avg = p;
	else
		x->avg = (x->avg * ((x->pt-(float)1.0)/(x->pt))) + (p * ((float)1.0/x->pt));

	outlet_float(x->x_obj.ob_outlet, x->avg);
}

void *gsm_chan_new(void)
{
	t_gsm_chan *x = (t_gsm_chan*)pd_new(c_gsm_chan);
	
	floatinlet_new(&x->x_obj, &x->chan);		// second inlet: channel number
	outlet_new(&x->x_obj, gensym("float"));		// outlet: power

	return (void*)x;
}

void gsm_chan_bang(t_gsm_chan *x)
{
	BASE			*base;
	float			p = 0.0;
	unsigned int	chan = (unsigned int)x->chan;

	// search for channel
	base = _getBase();		// lock
	while (base)
	{
		if (base->channel == chan)
		{
			p = (float)base->p;
			break;
		}
		base = base->pNext;
	}
	_baseUnlock();			// unlock

	outlet_float(x->x_obj.ob_outlet, p);
}

void *gsm_loc_new(void)
{
	t_gsm_loc *x = (t_gsm_loc*)pd_new(c_gsm_loc);
	x->country_out = outlet_new(&x->x_obj, gensym("float"));	// first outlet: Mobile Country Code (MCC, Austria is 232)
	x->network_out = outlet_new(&x->x_obj, gensym("float"));	// second outlet: Mobile Network Code (MNC, yesss! is 5 in Austria)
	x->area_out = outlet_new(&x->x_obj, gensym("float"));		// third outlet: Location Area {Identifier,Code} (LAI/LAC)
	x->cell_out = outlet_new(&x->x_obj, gensym("float"));		// forth outlet: Cell Identifier	

	return (void*)x;
}

void gsm_loc_bang(t_gsm_loc *x)
{
	outlet_float(x->country_out, (float)g_locBuf.country);
	outlet_float(x->network_out, (float)g_locBuf.network);
	outlet_float(x->area_out, (float)g_locBuf.area);
	outlet_float(x->cell_out, (float)g_locBuf.cell);
}

void *gsm_num_new(void)
{
	t_gsm_num *x = (t_gsm_num*)pd_new(c_gsm_num);
	
	outlet_new(&x->x_obj, gensym("float"));		// outlet: number of channels

	return (void*)x;
}

void gsm_num_bang(t_gsm_num *x)
{
	BASE			*base;
	float			num = 0.0;

	// iterate channels
	base = _getBase();		// lock
	while (base)
	{
		num++;
		base = base->pNext;
	}
	_baseUnlock();			// unlock

	outlet_float(x->x_obj.ob_outlet, num);
}

void *gsm_sort_new(void)
{
	t_gsm_sort *x = (t_gsm_sort*)pd_new(c_gsm_sort);
	
	floatinlet_new(&x->x_obj, &x->num);						// second inlet: zero-based index
	x->p_out = outlet_new(&x->x_obj, gensym("float"));		// first outlet: power
	x->chan_out = outlet_new(&x->x_obj, gensym("float"));	// second outlet: channel number
	x->changed_out = outlet_new(&x->x_obj, gensym("bang"));	// third outlet: bang if channel number has changed

	return (void*)x;
}

void gsm_sort_bang(t_gsm_sort *x)
{
	BASE			*base;
	unsigned int	i = 0;
	unsigned int	num = (unsigned int)x->num;

	// search for channel
	base = _getBase();		// lock
	while (base)
	{
		if (i == num)
			break;
		i++;
		base = base->pNext;
	}

	if (base)
	{
		float chan = (float)base->channel;
		outlet_float(x->p_out, (float)base->p);
		outlet_float(x->chan_out, chan);
		if (chan != x->prev_chan)
		{
			outlet_bang(x->changed_out);
			x->prev_chan = chan;
		}
	}
	else
	{
		outlet_float(x->p_out, 0.0);
		outlet_float(x->chan_out, 0.0);
	}

	_baseUnlock();			// unlock
}


void _baseUnlock()
{
	ReleaseMutex(g_hMutex);
}


BASE *_getBase(void)
{
	DWORD			dwWaitResult;

	dwWaitResult = WaitForSingleObject(g_hMutex, MUTEX_TIMEOUT);
	if (dwWaitResult == WAIT_OBJECT_0)
		return g_pBase;
	else if (dwWaitResult == WAIT_TIMEOUT)
	{
		post("gsm: could not aquire mutex in time");		// DEBUG
	}
	return NULL;		// error: could not aquire mutex in time
}


bool _getNetmonState(void)
{
	DWORD			temp;

	GetExitCodeThread(g_hThread, &temp);
	if (temp == STILL_ACTIVE)
		return true;
	else
	{
		// clean up
		CloseHandle(g_thread.hSignal);
		g_thread.hSignal = 0;
		CloseHandle(g_hThread);
		g_hThread = 0;
		return false;
	}
}


bool _startNetmonThread(unsigned int port)
{
	DWORD			dwThreadId;

	// prepare NMTHREAD struct
	g_thread.base = &g_pBase;
	g_thread.hMutex = g_hMutex;
	g_thread.hSignal = CreateEvent(NULL, false, false, NULL);
	g_thread.loc = &g_locBuf;
	g_thread.port = port;

	// create thread
	g_hThread = CreateThread(NULL, 0, netmonThread, &g_thread, 0, &dwThreadId);
	if (g_hThread == NULL)
		return false;		// error: cannot create thread
	else
		return true;
}

DWORD WINAPI netmonThread(LPVOID lpParam)
{
	BASE			tempBaseBuf = { 0 };
	BASE			*cur = &tempBaseBuf;
	BASE			*pTemp, *pTemp2;
	DWORD			dwWaitResult;
	ERRORS			err;
	NMTHREAD		*thread = (NMTHREAD*)lpParam;
	unsigned int	prevChan = 0;

	err = connectMobile(thread->port);
	if (err != SUCCESS)
		return (int)(-1*err);		// error: connectMobile() failed

	do
	{
		err = getBasestations(thread->port, cur);
		if (err != SUCCESS)
		{
			// DEBUG
			//char debug[256];
			//sprintf_s(debug, sizeof(debug), "getBasestations() returned %u\n", (unsigned int)err);
			//OutputDebugString(debug);
			continue;
		}
		
		if (cur->channel != prevChan)
		{
			// call getLocation() if necessary
			err = getLocation(thread->port, thread->loc);
			// DEBUG
			//if (err != SUCCESS)
			//{
			//	char debug[256];
			//	sprintf_s(debug, sizeof(debug), "getLocation() returned %u\n", (unsigned int)err);
			//	OutputDebugString(debug);
			//}
		}

		dwWaitResult = WaitForSingleObject(thread->hMutex, MUTEX_TIMEOUT);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			__try {
				if (thread->base)			// should always be the case
				{
					pTemp2 = *(thread->base);
					if (pTemp2->pNext)
					{
						// free the list currently allocated
						pTemp = pTemp2->pNext;
						pTemp2->pNext = NULL;
						while (pTemp)
						{
							pTemp2 = pTemp->pNext;
							free(pTemp);
							pTemp = pTemp2;
						}
					}
					// switch buffers
					pTemp = *(thread->base);
					*(thread->base) = cur;
					cur = pTemp;
				}
			}
			__finally
			{
				ReleaseMutex(thread->hMutex);
			}
		}
		// DEBUG
		//else if (dwWaitResult == WAIT_TIMEOUT)
		//{
		//	char debug[256];
		//	sprintf_s(debug, sizeof(debug), "getBasestations() returned %u\n", (unsigned int)err);
		//	OutputDebugString(debug);
		//}

		// check signal
		dwWaitResult = WaitForSingleObject(thread->hSignal, 0L);
		if (dwWaitResult == WAIT_OBJECT_0)
			break;
	}
	while (true);


	// cleanup
	dwWaitResult = WaitForSingleObject(thread->hMutex, INFINITE);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		// free linked list
		pTemp = *(thread->base);
		pTemp = pTemp->pNext;
		while (pTemp)
		{
			pTemp2 = pTemp->pNext;
			free(pTemp);
			pTemp = pTemp2;
		}

		// switch pointer to global buffer if necessary
		if (*(thread->base) == &tempBaseBuf)
		{
			pTemp = *(thread->base);
			*(thread->base) = cur;
			cur = pTemp;
		}

		// set global buffer to sane values
		pTemp = *(thread->base);
		pTemp->channel = 0;
		pTemp->p = 0;
		pTemp->pNext = NULL;
	}
	ReleaseMutex(thread->hMutex);

	disconnectMobile(thread->port);

	return 0;
}


void _stopNetmonThread(void)
{
	DWORD			ret;

	// send event (polite)
	SetEvent(g_thread.hSignal);
	// wait for thread to exit
	ret = WaitForSingleObject(g_hThread, 1000);
	if (ret == WAIT_TIMEOUT)
	{
		// kill thread the hard way (quite dangerous)
		TerminateThread(g_hThread, -19);
		post("gsm: terminating thread the hard way");
	}
	// clean up
	CloseHandle(g_thread.hSignal);
	g_thread.hSignal = 0;
	CloseHandle(g_hThread);
	g_hThread = 0;
}