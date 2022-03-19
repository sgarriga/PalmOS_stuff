/***********************************************************************
 *
 **********************************************************************/

//#define TEST

#include <Pilot.h>
#include <SysEvtMgr.h>
#include "PalmBoidsRsc.h"

#ifdef TEST
#define BOID_COUNT 3
#else
#define BOID_COUNT 10
#endif

#define INTERVAL   3

// Screen draw area for boids
#define MIN_X 0
#define MAX_X 159
#define MIN_Y 16
#define MAX_Y 140

// speed limits
#define MIN_V 1
#define MAX_V 5

// How close can they get
#define MIN_SPACING 3

#define C_ACCN          10
#define PV_ACCN          8

/***********************************************************************
 *
 *   Entry Points
 *
 ***********************************************************************/
static unsigned int root(unsigned long val);

/***********************************************************************
 *
 *   Internal Structures
 *
 ***********************************************************************/
typedef struct
{
    short x;
    short y;
} vector;

typedef struct
{
    vector pos;
    vector vel;
} boidT;


/***********************************************************************
 *
 *   Global variables
 *
 ***********************************************************************/
int interval = evtWaitForever;

/***********************************************************************
 *
 *   Internal Constants
 *
 ***********************************************************************/
#define appFileCreator			   'SGpb'
#define appVersionNum              0x01
#define appPrefID                  0x00
#define appPrefVersionNum          0x01


// Define the minimum OS version we support
#define ourMinVersion	sysMakeROMVersion(2,0,0,sysROMStageRelease,0)

#define random(lo, hi)      (SysRandom(0) % (hi - lo)) + lo

#define vSetXY(a, b, c)     (a).x = (b); (a).y = (c)
#define vSet(a, b)          (a).x = (b).x; (a).y = (b).y
#define vAdd(a, b)          (a).x += (b).x; (a).y += (b).y
#define vSub(a, b)          (a).x -= (b).x; (a).y -= (b).y
#define vDiv(a, b)          (a).x = (a).x/(b); (a).y = (a).y/(b)
#define vMul(a, b)          (a).x = (a).x*(b); (a).y = (a).y*(b)
#define vMag(a)             root(((a).x * (a).x) + ((a).y * (a).y))


#define abs(a)              ((a) < 0 ? -(a) : (a))

/***********************************************************************
 *
 *   Internal Functions
 *
 ***********************************************************************/

static unsigned int root(unsigned long val) 
{
  unsigned int temp, g=0;

  if (val >= 0x40000000) {
    g = 0x8000; 
    val -= 0x40000000;
  }

#define INNER_MBGSQRT(s)                      \
  temp = (g << (s)) + (1 << ((s) * 2 - 2));   \
  if (val >= temp) {                          \
    g += 1 << ((s)-1);                        \
    val -= temp;                              \
  }

  INNER_MBGSQRT (15)
  INNER_MBGSQRT (14)
  INNER_MBGSQRT (13)
  INNER_MBGSQRT (12)
  INNER_MBGSQRT (11)
  INNER_MBGSQRT (10)
  INNER_MBGSQRT ( 9)
  INNER_MBGSQRT ( 8)
  INNER_MBGSQRT ( 7)
  INNER_MBGSQRT ( 6)
  INNER_MBGSQRT ( 5)
  INNER_MBGSQRT ( 4)
  INNER_MBGSQRT ( 3)
  INNER_MBGSQRT ( 2)

#undef INNER_MBGSQRT

  temp = g+g+1;
  if (val >= temp) g++;
  return g;
}
 
static short vDiff(vector x, vector y)
{
    vector v;
    
    vSet(v, x);
    vSub(v, y);
    
    return root(vMag(v));
}

static void plot(boidT b)
{
    if ((b.pos.x > MIN_X) && (b.pos.x < MAX_X) && 
        (b.pos.y > MIN_Y) && (b.pos.y < MAX_Y))
    {
        WinDrawLine(b.pos.x-1, b.pos.y-1, b.pos.x+1, b.pos.y+1);
        WinDrawLine(b.pos.x-1, b.pos.y+1, b.pos.x+1, b.pos.y-1);
        WinDrawLine(b.pos.x, b.pos.y, b.pos.x-b.vel.x, b.pos.y-b.vel.y);
    }
}

static void unplot(boidT b)
{
    if ((b.pos.x > MIN_X) && (b.pos.x < MAX_X) && 
        (b.pos.y > MIN_Y) && (b.pos.y < MAX_Y))
    {
        WinEraseLine(b.pos.x-1, b.pos.y-1, b.pos.x+1, b.pos.y+1);
        WinEraseLine(b.pos.x-1, b.pos.y+1, b.pos.x+1, b.pos.y-1);
        WinEraseLine(b.pos.x, b.pos.y, b.pos.x-b.vel.x, b.pos.y-b.vel.y);
    }
}

static void iniBoids(boidT b[])
{
#ifndef TEST
    int i;
    for (i = 0; i < BOID_COUNT; i++)
    {
        b[i].pos.x = random(MIN_X, MAX_X);
        b[i].pos.y = random(MIN_Y, MAX_Y);
        b[i].vel.x = random(MIN_V, MAX_V) * ((SysRandom(0) % 1) ? -1 : 1);
        b[i].vel.y = random(MIN_V, MAX_V) * ((SysRandom(0) % 1) ? -1 : 1);
        plot(b[i]);
    }
#else
    b[0].pos.x = 40;
    b[0].pos.y = 50;
    b[0].vel.x = -3;
    b[0].vel.y = 0;
    
    b[1].pos.x = 50;
    b[1].pos.y = 100;
    b[1].vel.x = 3;
    b[1].vel.y = 3;
    
    b[2].pos.x = 130;
    b[2].pos.y = 120;
    b[2].vel.x = 0;
    b[2].vel.y = 3;
#endif
}

static void r1(boidT b[], short id, vector *v)
{
    int i;
    vector tmp;

    vSetXY(*v, 0, 0);
    
    for (i = 0; i < BOID_COUNT; i++)
    {
        if (i != id)
        {
            vAdd(*v, b[i].pos);
        }
    }
    vDiv(*v, BOID_COUNT - 1);
    
    vSet(tmp, b[id].pos); 
    vSub(tmp, *v);
    vSet(*v, tmp);
    
    vDiv(*v, C_ACCN);
}

static void r2(boidT b[], short id, vector *v)
{
    int i;
    vector tmp;

    vSetXY(*v, 0, 0);
    
    for (i = 0; i < BOID_COUNT; i++)
    {
        if (i != id)
            if (vDiff(b[id].pos, b[i].pos) < MIN_SPACING)
            {
                vSet(tmp, b[id].pos);
                vSub(tmp, b[i].pos);
                vSub(*v, tmp);
            }
    }

}

static void r3(boidT b[], short id, vector *v)
{
    int i;

    vSetXY(*v, 0, 0);
    
    for (i = 0; i < BOID_COUNT; i++)
    {
        if (i != id)
        {
            vAdd(*v, b[i].vel);
        }
    }
    
    vDiv(*v, BOID_COUNT - 1);
    vSub(*v, b[id].vel);
    vDiv(*v, PV_ACCN);
}

static void r4(boidT b[], short id, vector *v)
{
    vSetXY(*v, 0, 0);
    if (b[id].pos.x + b[id].vel.x < MIN_X) 
        v->x = MAX_V;    
    if (b[id].pos.y + b[id].vel.y < MIN_Y) 
        v->y = MAX_V;    
    if (b[id].pos.x + b[id].vel.x > MAX_X) 
        v->x = -MAX_V;    
    if (b[id].pos.y + b[id].vel.y > MAX_Y) 
        v->y = -MAX_V;                
}

static void limitV(boidT *b)
{
    if (vMag(b->vel) > MAX_V)
    {
        vMul(b->vel, MAX_V);    
        vDiv(b->vel, vMag(b->vel));
    }
    else if (vMag(b->vel) < MIN_V)
    {
        vMul(b->vel, 2);
    }
    
    if (vMag(b->vel) < MIN_V)
    {
        if (((b->vel.x >= 0 ) && (b->vel.x < MIN_V)) ||
            ((b->vel.x < 0 ) && (b->vel.x > -MIN_V)))
            b->vel.x = MIN_V * ((SysRandom(0) % 1) ? -1 : 1);
        if (((b->vel.y >= 0 ) && (b->vel.y < MIN_V)) ||
            ((b->vel.y < 0 ) && (b->vel.y > -MIN_V)))
            b->vel.y = MIN_V * ((SysRandom(0) % 1) ? -1 : 1);
    }
}


static void updBoids(boidT b[])
{
    int i;
    vector v1, v2, v3, v4;
    boidT bn[BOID_COUNT];
    
    MemMove(bn, b, sizeof(bn));
    
    for (i = 0; i < BOID_COUNT; i++)
    {
#ifndef TEST
        unplot(b[i]);
#endif
        
        r1(b, i, &v1);
        r2(b, i, &v2);
        r3(b, i, &v3);
        r4(b, i, &v4);

#ifdef TEST        
        {
            char xx[80];
            StrPrintF(xx, "R1 B[%d] = %d,%d", 
            i,
            v1.x, v1.y);
            FrmCustomAlert(DebugAlert, xx, NULL, NULL);

            StrPrintF(xx, "R2 B[%d] = %d,%d", 
            i,
            v2.x, v2.y);
            FrmCustomAlert(DebugAlert, xx, NULL, NULL);

            StrPrintF(xx, "R3 B[%d] = %d,%d", 
            i,
            v3.x, v3.y);
            FrmCustomAlert(DebugAlert, xx, NULL, NULL);

            StrPrintF(xx, "R4 B[%d] = %d,%d", 
            i,
            v4.x, v4.y);
            FrmCustomAlert(DebugAlert, xx, NULL, NULL);
        }
#endif

        vAdd(bn[i].vel, v1);
        vAdd(bn[i].vel, v2);
        vAdd(bn[i].vel, v3);
        vAdd(bn[i].vel, v4);
        
        limitV(&bn[i]);
        
#ifdef TEST        
        {
            char xx[80];
            StrPrintF(xx, "Vn B[%d] = %d,%d", 
            i,
            bn[i].vel.x, bn[i].vel.y);
            FrmCustomAlert(DebugAlert, xx, NULL, NULL);
        }
#endif
                
        vAdd(bn[i].pos, bn[i].vel);
}

    MemMove(b, bn, sizeof(bn));

#ifndef TEST        
    for (i = 0; i < BOID_COUNT; i++)
    {
        plot(b[i]);
    }
#endif

}


static Err RomVersionCompatible(DWord requiredVersion, Word launchFlags)
{
	DWord romVersion;

	// See if we're on in minimum required version of the ROM or later.
	FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
	if (romVersion < requiredVersion)
		{
		if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
			(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
			{
			FrmAlert (RomIncompatibleAlert);
		
			// Pilot 1.0 will continuously relaunch this app unless we switch to 
			// another safe one.
			if (romVersion < sysMakeROMVersion(2,0,0,sysROMStageRelease,0))
				AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
			}
		
		return (sysErrRomIncompatible);
		}

	return (0);
}


static Boolean MainFormDoCommand(Word command)
{
	Boolean handled = false;


	switch (command)
		{
		case MainOptionsAboutPalmBoids:
			MenuEraseStatus (0);
			AbtShowAbout (appFileCreator);
			handled = true;
			break;

		}
	return handled;
}


static Boolean MainFormHandleEvent(EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;
    static boidT   boids[BOID_COUNT];

	switch (eventP->eType) 
		{
		case menuEvent:
			return MainFormDoCommand(eventP->data.menu.itemID);

		case frmOpenEvent:
			frmP = FrmGetActiveForm();
			iniBoids(boids);
			FrmDrawForm (frmP);
			handled = true;
			break;
			
		case ctlSelectEvent:
		{
		    switch (eventP->data.ctlSelect.controlID)
		    {
		        case MainOKButton :
		            if (interval == INTERVAL)
		                interval = evtWaitForever;
		            else
		                interval = INTERVAL;
    	            handled = true;
	                break;
	        }
	    }
			
	    case nilEvent:
	        updBoids(boids);
	        handled = true;
	        break;

		default:
			break;
		
		}
	
	return handled;
}


static Boolean AppHandleEvent( EventPtr eventP)
{
	Word formId;
	FormPtr frmP;


	if (eventP->eType == frmLoadEvent)
		{
		// Load the form resource.
		formId = eventP->data.frmLoad.formID;
		frmP = FrmInitForm(formId);
		FrmSetActiveForm(frmP);

		// Set the event handler for the form.  The handler of the currently
		// active form is called by FrmHandleEvent each time is receives an
		// event.
		switch (formId)
			{
			case MainForm:
				FrmSetEventHandler(frmP, MainFormHandleEvent);
				break;

			default:
				break;

			}
		return true;
		}
	
	return false;
}


static void AppEventLoop(void)
{
	Word error;
	EventType event;


	do {
		EvtGetEvent(&event, interval);	
		if (! SysHandleEvent(&event))
			if (! MenuHandleEvent(0, &event, &error))
				if (! AppHandleEvent(&event))
					FrmDispatchEvent(&event);

	} while (event.eType != appStopEvent);
}


static Err AppStart(void)
{
   return 0;
}


static void AppStop(void)
{

}



static DWord PalmBoidsPilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
	Err error;


	error = RomVersionCompatible (ourMinVersion, launchFlags);
	if (error) return (error);


	switch (cmd)
		{
		case sysAppLaunchCmdNormalLaunch:
			error = AppStart();
			if (error) 
				return error;
				
			FrmGotoForm(MainForm);
			AppEventLoop();
			AppStop();
			break;

		default:
			break;

		}
	
	return 0;
}


DWord PilotMain( Word cmd, Ptr cmdPBP, Word launchFlags)
{
    return PalmBoidsPilotMain(cmd, cmdPBP, launchFlags);
}

