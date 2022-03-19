#include <Pilot.h>
#include <SysEvtMgr.h>

#include "Boidz_res.h"

#include "Vector.h"
#include "Boid.h"
#include "boidLimits.h"


/***********************************************************************
 *
 *   Internal Structures
 *
 ***********************************************************************/
struct BoidzPreferenceType
{
	Byte replaceme;
} ;

struct BoidzAppInfoType
{
	Byte replaceme;
} ;
	
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
#define version20                  sysMakeROMVersion(2,0,0,sysROMStageRelease,0)

/***********************************************************************
 *
 * FUNCTION:    RomVersionCompatible
 *
 * DESCRIPTION: This routine checks that a ROM version is meet your
 *              minimum requirement.
 *
 * PARAMETERS:  requiredVersion - minimum rom version required
 *                                (see sysFtrNumROMVersion in SystemMgr.h 
 *                                for format)
 *              launchFlags     - flags that indicate if the application 
 *                                UI is initialized.
 *
 * RETURNED:    error code or zero if rom is compatible
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
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
			if (romVersion < version20)
			{
			    AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
			}
		}
		
		return (sysErrRomIncompatible);
	}

	return (0);
}


/***********************************************************************
 *
 * FUNCTION:    GetObjectPtr
 *
 * DESCRIPTION: This routine returns a pointer to an object in the current
 *              form.
 *
 * PARAMETERS:  formId - id of the form to display
 *
 * RETURNED:    VoidPtr
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static VoidPtr GetObjectPtr(Word objectID)
{
	FormPtr frmP;

	frmP = FrmGetActiveForm();

	return (FrmGetObjectPtr(frmP, FrmGetObjectIndex(frmP, objectID)));
}

/***********************************************************************
 *
 * FUNCTION:    rule1
 *
 * DESCRIPTION: Boids fly toward to the center of flock
 *
 * PARAMETERS:  set of boids, id of current boid
 *
 * RETURNED:    Vector (directional change generated)
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Vector rule1(Boid b[], short id)
{
    int i;
    Vector pc = Vector(0, 0);
   
    for (i = 0; i < BOID_COUNT; i++)
    {
        if (i != id)
        {
            pc = pc + b[i].pos;
        }
    }
    pc = (pc / (BOID_COUNT-1));
    
    return (pc - b[id].pos) / C_ACCN;
}

/***********************************************************************
 *
 * FUNCTION:    rule2
 *
 * DESCRIPTION: Boids avoid collisions with other boids
 *
 * PARAMETERS:  set of boids, id of current boid
 *
 * RETURNED:    Vector (directional change generated)
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Vector rule2(Boid b[], short id)
{
    int i;
    Vector c = Vector(0, 0);

    for (i = 0; i < BOID_COUNT; i++)
    {
        if (i != id)
        {
            Vector d = b[id].pos - b[i].pos; 
            if (d.magnitude() < MIN_SPACING)
            {
                c = c - d;
            }
        }
    }
    
    return c;
}

/***********************************************************************
 *
 * FUNCTION:    rule3
 *
 * DESCRIPTION: Boids try to match speed with near boids
 *
 * PARAMETERS:  set of boids, id of current boid
 *
 * RETURNED:    Vector (directional change generated)
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Vector rule3(Boid b[], short id)
{
    int i;
    Vector pv = Vector(0, 0);
   
    for (i = 0; i < BOID_COUNT; i++)
    {
        if (i != id)
        {
            pv = pv + b[i].vel;
        }
    }
    
    pv = pv / (BOID_COUNT-1);
    
    return (pv - b[id].vel) / PV_ACCN;
}

/***********************************************************************
 *
 * FUNCTION:    rule_bound
 *
 * DESCRIPTION: Boids try to stay on screen
 *
 * PARAMETERS:  current boid
 *
 * RETURNED:    Vector (directional change generated)
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Vector rule_bound(Boid b)
{
    Vector v = Vector(0, 0);

    if (b.pos.x < MIN_X) 
        v.x = MAX_V;    
    else if (b.pos.x > MAX_X) 
        v.x = -MAX_V;    
    
    if (b.pos.y < MIN_Y) 
        v.y = MAX_V;    
    else if (b.pos.y > MAX_Y) 
        v.y = -MAX_V;                
    
    return v;
}

/***********************************************************************
 *
 * FUNCTION:    rule_tend_to
 *
 * DESCRIPTION: Boids are attracted to the middle of the screen
 *
 * PARAMETERS:  current boid
 *
 * RETURNED:    Vector (directional change generated)
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Vector rule_tend_to(Boid b)
{
    int cx = MIN_X + ((MAX_X - MIN_X) / 2);
    int cy = MIN_Y + ((MAX_Y - MIN_Y) / 2);    
    Vector p = Vector(cx, cy);

    return ((p - b.pos) / C_ACCN);
}

/***********************************************************************
 *
 * FUNCTION:    limit_vel
 *
 * DESCRIPTION: keep velocities 'realistic'
 *
 * PARAMETERS:  current boid
 *
 * RETURNED:    Vector (new vel for boid)
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Vector limit_vel(Boid b)
{
    Vector v = b.vel;

    if (v.magnitude() > (int) (1.5 * MAX_V))
    {
        v.ceil(MAX_V, MAX_V);
        v.floor(-MAX_V, -MAX_V);
    }
    //    v = (v * MAX_V) / v.magnitude();

    return v;
}

/***********************************************************************
 *
 * FUNCTION:    updateBoids
 *
 * DESCRIPTION: recalculate and plot all the boids positions
 *
 * PARAMETERS:  set of boids
 *
 * RETURNED:    -
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static void updateBoids(Boid b[])
{
    int i;
    Vector v1, v2, v3, v_b, v_t;

    for (i = 0; i < BOID_COUNT; i++)
    {
        b[i].unplot();
        
        v1 = rule1(b, i);
        v2 = rule2(b, i);
        v3 = rule3(b, i);
        
        v_b = rule_bound(b[i]);
        
        v_t = rule_tend_to(b[i]);

        b[i].vel = b[i].vel + v1 + v2 + v3 + v_b + v_t;
        
        b[i].vel = limit_vel(b[i]);
                       
        b[i].pos = b[i].pos + b[i].vel;
        
        b[i].plot();
    }

}

/***********************************************************************
 *
 * FUNCTION:    initBoids
 *
 * DESCRIPTION: setup the boids initial positions
 *
 * PARAMETERS:  set of boids
 *
 * RETURNED:    -
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static void initBoids(Boid b[])
{
#define random(lo, hi)      (SysRandom(0) % (hi - lo)) + lo
    int i;

    for (i = 0; i < BOID_COUNT; i++)
    {
        b[i].pos.x = random(MIN_X, MAX_X);
        b[i].pos.y = random(MIN_Y, MAX_Y);
        b[i].vel.x = random(-MAX_V, MAX_V);
        b[i].vel.y = random(-MAX_V, MAX_V);
        
        b[i].plot();
    }
#undef random
}


/***********************************************************************
 *
 * FUNCTION:    MainFormInit
 *
 * DESCRIPTION: This routine initializes the MainForm form.
 *
 * PARAMETERS:  frm - pointer to the MainForm form.
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static void MainFormInit(FormPtr frmP)
{
}


/***********************************************************************
 *
 * FUNCTION:    MainFormDoCommand
 *
 * DESCRIPTION: This routine performs the menu command specified.
 *
 * PARAMETERS:  command  - menu item id
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Boolean MainFormDoCommand(Word command)
{
	Boolean handled = false;


	switch (command)
	{
		case MainOptionsAboutBoidz:
    		//MenuEraseStatus (0);
    		void **verH = DmGet1Resource('tver', 1000);
            char *ver  = (char *) MemHandleLock(verH);
			FrmCustomAlert(AboutAlert, "Boidz", ver, NULL);			
			MemHandleUnlock(verH);
			DmReleaseResource(verH);
			handled = true;
			break;

	}
	return handled;
}


/***********************************************************************
 *
 * FUNCTION:    MainFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the 
 *              "MainForm" of this application.
 *
 * PARAMETERS:  eventP  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Boolean MainFormHandleEvent(EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;
    static Boid boids[BOID_COUNT];

	switch (eventP->eType) 
	{
		case menuEvent:
			return MainFormDoCommand(eventP->data.menu.itemID);

		case frmOpenEvent:
			frmP = FrmGetActiveForm();
			MainFormInit(frmP);
			initBoids(boids);
			FrmDrawForm (frmP);
			handled = true;
			break;
			
		case ctlSelectEvent:
		{
		    switch (eventP->data.ctlSelect.controlID)
		    {
		        case MainStopGoButton :
		            if (interval == INTERVAL)
		                interval = evtWaitForever;
		            else
		                interval = INTERVAL;
    	            handled = true;
	                break;
	        }
	    }
			
	    case nilEvent:
	        if (interval == INTERVAL)
    	        updateBoids(boids);
	        handled = true;
	        break;

		default:
			break;
		
	}
	
	return handled;
}

/***********************************************************************
 *
 * FUNCTION:    AppHandleEvent
 *
 * DESCRIPTION: This routine loads form resources and set the event
 *              handler for the form loaded.
 *
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
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
			{
				FrmSetEventHandler(frmP, MainFormHandleEvent);
			}
			break;

			default:
//				ErrFatalDisplay("Invalid Form Load Event");
				break;

			}
		return true;
		}
	
	return false;
}


/***********************************************************************
 *
 * FUNCTION:    AppEventLoop
 *
 * DESCRIPTION: This routine is the event loop for the application.  
 *
 * PARAMETERS:  nothing
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static void AppEventLoop(void)
{
	Word error;
	EventType event;


	do 
	{
		EvtGetEvent(&event, interval);
				
		if (!SysHandleEvent(&event))
			if (!MenuHandleEvent(0, &event, &error))
				if (!AppHandleEvent(&event))
					FrmDispatchEvent(&event);

	} 
	while (event.eType != appStopEvent);
}


/***********************************************************************
 *
 * FUNCTION:     AppStart
 *
 * DESCRIPTION:  Get the current application's preferences.
 *
 * PARAMETERS:   nothing
 *
 * RETURNED:     Err value 0 if nothing went wrong
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static Err AppStart(void)
{
    BoidzPreferenceType prefs;
    Word prefsSize;

	// Read the saved preferences / saved-state information.
	prefsSize = sizeof(BoidzPreferenceType);
	if (PrefGetAppPreferences(appFileCreator, appPrefID, &prefs, &prefsSize, true) != 
		noPreferenceFound)
	{
	}
	
   return 0;
}


/***********************************************************************
 *
 * FUNCTION:    AppStop
 *
 * DESCRIPTION: Save the current state of the application.
 *
 * PARAMETERS:  nothing
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static void AppStop(void)
{
   BoidzPreferenceType prefs;
   
	// Write the saved preferences / saved-state information.  This data 
	// will be backed up during a HotSync.
	PrefSetAppPreferences (appFileCreator, appPrefID, appPrefVersionNum, 
		&prefs, sizeof (prefs), true);
}



/***********************************************************************
 *
 * FUNCTION:    BoidzPilotMain
 *
 * DESCRIPTION: This is the main entry point for the application.
 * PARAMETERS:  cmd - word value specifying the launch code. 
 *              cmdPB - pointer to a structure that is associated with the launch code. 
 *              launchFlags -  word value providing extra information about the launch.
 *
 * RETURNED:    Result of launch
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
static DWord BoidzPilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
	Err error;


	error = RomVersionCompatible (version20, launchFlags);
	if (error) 
        return (error);


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


/***********************************************************************
 *
 * FUNCTION:    PilotMain
 *
 * DESCRIPTION: This is the main entry point for the application.
 *
 * PARAMETERS:  cmd - word value specifying the launch code. 
 *              cmdPB - pointer to a structure that is associated with the launch code. 
 *              launchFlags -  word value providing extra information about the launch.
 * RETURNED:    Result of launch
 *
 * REVISION HISTORY:
 *
 *
 ***********************************************************************/
DWord PilotMain( Word cmd, Ptr cmdPBP, Word launchFlags)
{
    return BoidzPilotMain(cmd, cmdPBP, launchFlags);
}
