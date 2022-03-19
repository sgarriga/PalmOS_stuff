#include <Pilot.h>
#include <SysEvtMgr.h>
#include <FeatureMgr.h>			//	Needed to detect ROM versions
#include <MemoryMgr.h>
#include <NewFloatMgr.h>
#include <MathLib.h>
#include <WindowNew.h>

#include "Gun Smoke_res.h"

// number of rounds per clip
#define MAX_ROUNDS	10
// lowest and upper screen line used for target
#define TARGET_UPPER  11
#define TARGET_BOTTOM 139

//Midpoint of screen
#define mX  80
#define mY  75

#define BULL_RAD  10

/***********************************************************************
 *   Entry Points
 ***********************************************************************/


/***********************************************************************
 *   Internal Structures
 ***********************************************************************/
typedef enum targetType { bullsEye, 
                          runningMan } target_t;

typedef enum drawType { draw, erase } draw_t;
                          
typedef enum problemType { none = 0,
                           jerk = 1,
                           riding = 2,
                           squeeze = 3,
                           heeling = 4,
                           thumbing = 5,
                           breaking = 6,
                           lobster = 7,
                           random1 = 8,
                           random2 = 9} problem_t;

typedef struct { target_t target; } Gs_PreferenceType;

typedef enum axisType { x = 0, y = 1 } axis_t;
typedef short point[2];

/***********************************************************************
 *   Global variables
 ***********************************************************************/
static DmOpenRef Gs_DB;
static FontID AppFontID = stdFont;
static Gs_PreferenceType prefs;
static point rounds[MAX_ROUNDS];
static short round_cnt;
static char debug[30];
static short aveX = 0;
static short aveY = 0;

static char anals[10][256] = 
{"Your average hit seems to be near the bull - don't worry, practice makes perfect.",
 "You may be jerking the trigger - remember to squeeze slowly and follow through.",
 "You seem to be anticipating the recoil -  relax, squeeze slowly and follow through.",
 "Do you have the correct part of your finger on the trigger? - dry-fire practice can help.",
 "I think you're anticipating the shot - try some dry-firing, and try to relax.",
 "Your thumb may be pushing the frame as you fire - examine and adjust your grip.",
 "You might be anticipating the recoil and tipping your wrist downwards - try some dry-firing.",
 "It looks like you're tightening your grip as you fire - practice dry-firing and your follow through",
 "Your grouping does not suggest a common problem - all I can suggest is talk to an expert.",
 "There is no obvious pattern - all I can suggest is talk to an expert."};

/***********************************************************************
 *   Internal Constants
 ***********************************************************************/
#define appFileCreator             'SGGS'
#define appVersionNum              0x01
#define appPrefID                  0x00
#define appPrefVersionNum          0x01
#define appDBName                  "Gs_DB"
#define appDBType                  'DATA'
// Version Checking define
#define version30                  0x03000000

const long double pi = 3.14159265358979323846L;
// Degree:Radians
const long double DtoR = (pi/180);

/***********************************************************************
 *   Internal Functions
 ***********************************************************************/
static double l_atan(short opp, short adj_b, short adj_c)
{
    double adj_a;
    
    adj_a = sqrt((adj_b * adj_b) + (adj_c * adj_c));

    return atan(opp/adj_a)/DtoR;    
}

static void plot (short x, short y, draw_t dt)
{
    if (dt == draw)
        WinDrawLine(x, y, x, y);
    else
        WinEraseLine(x, y, x, y);
}

static void rectangle (short x1, short y1, short x2, short y2, draw_t dt)
{
    if (dt == draw)
    {
        WinDrawLine(x1, y1, x2, y1);
        WinDrawLine(x1, y1, x1, y2);
        WinDrawLine(x1, y2, x2, y2);
        WinDrawLine(x2, y2, x2, y1);
    }
    else
    {
        WinEraseLine(x1, y1, x2, y1);
        WinEraseLine(x1, y1, x1, y2);
        WinEraseLine(x1, y2, x2, y2);
        WinEraseLine(x2, y2, x2, y1);
    }
}

static void circle (short cx, short cy, short cr, draw_t dt)
{
        short x, y, r2;

        r2 = cr * cr;
        plot( cx, cy + cr, dt);
        plot( cx, cy - cr, dt);
        plot( cx + cr, cy, dt);
        plot( cx - cr, cy, dt);

        y = cr;
        x = 1;
        y = (short) (sqrt(r2 - 1) + 0.5);
        while (x < y) {
            plot( cx + x, cy + y, dt);
            plot( cx + x, cy - y, dt);
            plot( cx - x, cy + y, dt);
            plot( cx - x, cy - y, dt);
            plot( cx + y, cy + x, dt);
            plot( cx + y, cy - x, dt);
            plot( cx - y, cy + x, dt);
            plot( cx - y, cy - x, dt);
            x += 1;
            y = (short) (sqrt(r2 - x*x) + 0.5);
        }
        if (x == y) {
            plot( cx + x, cy + y, dt);
            plot( cx + x, cy - y, dt);
            plot( cx - x, cy + y, dt);
            plot( cx - x, cy - y, dt);
        }
}


static void draw_screen ()
{
    short i;
    
    // Bisect Horiz & Vert
    WinDrawGrayLine( 0, mY, 159,  mY);
    WinDrawGrayLine(mX, 10,  mX, 139); 

    if (prefs.target == bullsEye)
    { 
        // fill center ring 
        for (i=0; i<BULL_RAD; i++)
            circle(mX, mY, i, draw); 
        // rings
        for (i=1; i<(mY/BULL_RAD); i++)
            circle(mX, mY, BULL_RAD * i, draw);    
    }
    else
    {
        // fill center ring 
        for (i=0; i<BULL_RAD; i++)
            rectangle(mX-i, mY-i, mX+i, mY+i, draw); 
        // rings
        for (i=3; i<(mY/BULL_RAD); i++)
            rectangle(mX-(BULL_RAD*i/2), mY-(i*BULL_RAD),
                      mX+(BULL_RAD*i/2), mY+(BULL_RAD*i), draw);        
    }
    
}


static void undraw_screen (target_t target)
{
    short i;
    
    // Bisect Horiz & Vert
    WinEraseLine( 0, mY, 159,  mY);
    WinEraseLine(mX, 10,  mX, 139); 

    if (target == bullsEye)
    { 
        // fill center ring 
        for (i=0; i<BULL_RAD; i++)
            circle(mX, mY, i, erase); 
        // rings
        for (i=1; i<(mY/BULL_RAD); i++)
            circle(mX, mY, BULL_RAD * i, erase);    
    }
    else
    {
        // fill center ring 
        for (i=0; i<BULL_RAD; i++)
            rectangle(mX-i, mY-i, mX+i, mY+i, erase); 
        // rings
        for (i=3; i<(mY/BULL_RAD); i++)
            rectangle(mX-(BULL_RAD*i/2), mY-(i*BULL_RAD),
                      mX+(BULL_RAD*i/2), mY+(BULL_RAD*i), erase);        
    }
    
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
 ***********************************************************************/
static VoidPtr GetObjectPtr(Word objectID )
{
    FormPtr frmP;

    frmP = FrmGetActiveForm();
    return (FrmGetObjectPtr( frmP, FrmGetObjectIndex( frmP, objectID)));
}


/***********************************************************************
 *
 * FUNCTION:    RomVersionCompatible
 *
 * DESCRIPTION: This routine checks that a ROM version meets your
 *              minimum requirement.
 *
 * PARAMETERS:  requiredVersion - minimum rom version required
 *              launchFlags     - flags that indicate if the application 
 *                                UI is initialized.
 *
 * RETURNED:    error code or zero if rom is compatible
 *
 ***********************************************************************/
static Err RomVersionCompatible(DWord requiredVersion, Word launchFlags)
{
    DWord romVersion;

    // See if we have at least the minimum required version of the ROM or later.
    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
    if (romVersion < requiredVersion) {
        if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
            (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) {
            FrmAlert(ROMAlert);
            if (romVersion < version30) {
                AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
            }
        }
        return sysErrRomIncompatible;
    }
    return 0;
}


/***********************************************************************
 *
 * FUNCTION:    HandlePenDown
 *
 * DESCRIPTION:	Handles a pen down;
 *
 * PARAMETERS:	penX			-- display-relative x position
 *				penY			-- display-relative y position
 *
 * RETURNED:	true if handled; false if not
 *
 ***********************************************************************/
static Boolean HandlePenDown(Int penX, Int penY)
{
    int i;
    
    if ((round_cnt < MAX_ROUNDS) && 
        (penY <= TARGET_BOTTOM) && 
        (penY >= TARGET_UPPER))
    {
        // remove any old average box
        if ((aveX != 0) && (aveY != 0)) 
        {
            WinEraseLine(aveX-3,aveY-3,aveX-3,aveY+3);
            WinEraseLine(aveX-3,aveY-3,aveX+3,aveY-3);
            WinEraseLine(aveX+3,aveY+3,aveX-3,aveY+3);
            WinEraseLine(aveX+3,aveY+3,aveX+3,aveY-3);        
        }

        // record the new hit
        rounds[round_cnt][x] = penX;
        rounds[round_cnt][y] = penY;
        round_cnt++;
  
        // recalculate the average      
        aveX = 0;
        aveY = 0;   
     
        for (i=0; i<round_cnt; i++)
        {
            // re-ink all the hits (in case overwritten)
            WinDrawLine(rounds[i][x]-1,rounds[i][y],rounds[i][x]+1,rounds[i][y]);
            WinDrawLine(rounds[i][x],rounds[i][y]-1,rounds[i][x],rounds[i][y]+1);

            aveX += rounds[i][x];
            aveY += rounds[i][y];        
        }
        aveX /= round_cnt;
        aveY /= round_cnt;

        // draw average box
        WinDrawGrayLine(aveX-3,aveY-3,aveX-3,aveY+3);
        WinDrawGrayLine(aveX-3,aveY-3,aveX+3,aveY-3);
        WinDrawGrayLine(aveX+3,aveY+3,aveX-3,aveY+3);
        WinDrawGrayLine(aveX+3,aveY+3,aveX+3,aveY-3);        

        return true;
    }
    
	return false;
}


/***********************************************************************
 *
 * FUNCTION:    HandleReviewButton
 *
 * DESCRIPTION:	Reviews group
 *
 * PARAMETERS:	
 *
 * RETURNED:	true if handled; false if not
 *
 ***********************************************************************/
static Boolean HandleReviewButton()
{
    short i;
    double angle = 0.0;
    short X = 0;
    short Y = 0;
    problem_t prob = none;
    short spread = 0;
    short this_spread = 0;
    
    if (round_cnt == 0)
    {
        FrmAlert( NoneAlert );
        return true;
    }

    // remove hits
    for (i=0; i<round_cnt; i++)
    {
        // track max. distance from average
        this_spread = (short) sqrt(((aveX - rounds[i][x]) * (aveX - rounds[i][x])) +
                                   ((aveY - rounds[i][y]) * (aveY - rounds[i][y]))); 
                           
        if (this_spread > spread)
            spread = this_spread;
    }

    // Look at screen as 8 sectors (Q1+Q3 left&right, Q2+Q4 upper&lower)
    // and determine angle of grouping ( 12 o'clock = 0 degrees )
    //
    // \ Q1/
    // Q4\/Q2
    //   /\
    // / Q3 \
    
    X = aveX - mX;
    Y = mY - aveY;
    
    if (X <= Y) // Q4+Q1
    {
        if (X >= -Y) // Q1
        {
            if (X < 0) // LHS
            {
                angle = 360 - l_atan(-X, X, Y);
            }
            else       // RHS
            {
                angle = l_atan(X, X, Y);
            }
        }
        else         // Q4
        {
            if (Y < 0) // LOWER
            {
                angle = 270 - l_atan(-Y, X, Y);
            }
            else       // UPPER
            {
                angle = 270 + l_atan(Y, X, Y);
            }
        }
    }
    else // Q3+Q2
    {
        if (X >= -Y) // Q2
        {
            if (Y < 0) // LOWER
            {
                angle = 90 + l_atan(-Y, X, Y);
            }
            else       // UPPER
            {
                angle = 90 - l_atan(Y, X, Y);
            }
        }
        else         // Q4
        {
            if (X < 0) // LHS
            {
                angle = 180 + l_atan(-X, X, Y);
            }
            else       // RHS
            {
                angle = 180 - l_atan(X, X, Y);
            }
        }
    }

    // if we are within a bull sized grouping
    if (spread <= (BULL_RAD * 2))
    {
        // if we are in the bull area, all is well
        if ((fabs(aveX-mX) < BULL_RAD) && (fabs(aveY-mY) < BULL_RAD))
            prob = none;
    
        // 7:00 and  8:30
        else if (angle > 210 && angle <= 255)
            prob = jerk;

        // 9:30 and 12:00
        else if (angle > 285 && angle <= 360)
            prob = riding;

        // 8:30 and  9:30
        else if (angle > 255 && angle <= 285)
            prob = squeeze;

        // 1:00 and  2:30
        else if (angle >  30 && angle <=  75)
            prob = heeling;

        // 2:30 and  3:00
        else if (angle >  75 && angle <=  90)
            prob = thumbing;

        // 5:00 and  6:30
        else if (angle > 150 && angle <= 195)
            prob = breaking;

        // 3:00 and  5:00
        else if (angle >  90 && angle <= 150)
            prob = lobster;

        // random or not in one of out slices
        else
            prob = random1;
    }
    // too spread-out to be sure
    else
        prob = random2;
        
    // show analysis as an alert
    FrmCustomAlert(AnalysisAlert, anals[prob], NULL, NULL);
    
        // remove average box (if any)
    if ((aveX != 0) && (aveY != 0)) 
    {
        WinEraseLine(aveX-3,aveY-3,aveX-3,aveY+3);
        WinEraseLine(aveX-3,aveY-3,aveX+3,aveY-3);
        WinEraseLine(aveX+3,aveY+3,aveX-3,aveY+3);
        WinEraseLine(aveX+3,aveY+3,aveX+3,aveY-3);        
    }

    // remove hits
    for (i=0; i<round_cnt; i++)
    {
        WinEraseLine(rounds[i][x]-1,rounds[i][y],rounds[i][x]+1,rounds[i][y]);
        WinEraseLine(rounds[i][x],rounds[i][y]-1,rounds[i][x],rounds[i][y]+1);
    }
    
    // clear shot data        
    MemSet(rounds, sizeof(rounds), 0);
    round_cnt = 0;
    
    // redraw screen
    draw_screen();
    
	return true;
}


/***********************************************************************
 * FUNCTION:    ShowAbout
 *
 * DESCRIPTION: This procedure handles the Options ... About
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void ShowAbout(void)
{
	FrmAlert( AboutAlert );
}


/***********************************************************************
 * FUNCTION:    ShowOptions
 *
 * DESCRIPTION: This procedure handles the Options ... Options
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void ShowOptions(void)
{
	Word		buttonHit;
	FormPtr	    prefFormP;
	Word		controlID;
	
	// Create the form
	prefFormP = FrmInitForm( OptionsForm );
		
	switch( prefs.target )
	{
		case bullsEye:
			controlID = OptionsBullsEyePushButton;
			break;

		case runningMan:
			controlID = OptionsRunManPushButton;
			break;
	}
	FrmSetControlGroupSelection ( prefFormP, OptionsFormGroupID, controlID );
	
	// Put up the form
	buttonHit = FrmDoDialog( prefFormP );
	
	// Collect new setting
	if ( buttonHit == OptionsOKButton )
	{
		controlID = FrmGetObjectId( prefFormP,
				FrmGetControlGroupSelection(prefFormP, OptionsFormGroupID) );
		switch ( controlID )
		{
			case OptionsBullsEyePushButton:
				prefs.target = bullsEye;
				break;
				
			case OptionsRunManPushButton:
				prefs.target = runningMan;
				break;
				
		}
	}
	// Delete the form
	FrmDeleteForm( prefFormP );
}


/***********************************************************************
 *
 * FUNCTION:    GunSmokeFormDoCommand
 *
 * DESCRIPTION: This routine performs the menu command specified.
 *
 * PARAMETERS:  command  - menu item id
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Boolean GunSmokeFormDoCommand(Word command)
{
    Boolean handled = false;
    target_t old_target;

    switch (command) {
    
        case OptionsAbout:
            ShowAbout();
            handled = true;
            break;
        case OptionsOptions:
            old_target = prefs.target;
            ShowOptions();
            if (prefs.target != old_target)
            {
                undraw_screen(old_target);
                draw_screen();
            }
            handled = true;
            break;
        default:
            ErrNonFatalDisplay("Unexpected Event");
            break;

    }

    return handled;
}


/***********************************************************************
 * FUNCTION:    GunSmokeFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the
 *              "GunSmokeForm" of this application.
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Boolean GunSmokeFormHandleEvent( EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;

    switch (eventP->eType) {
        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) {
                case GunSmokeReviewButtonButton:
                    handled = HandleReviewButton();
                    break;
                default:
                    break;
            }
            break;
            
        case penDownEvent:
		    handled = HandlePenDown( eventP->screenX, eventP->screenY );
			break;

	
        case menuEvent:
            return GunSmokeFormDoCommand(eventP->data.menu.itemID);

        case frmOpenEvent:
            frmP = FrmGetActiveForm();
            FrmDrawForm ( frmP);
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
 ***********************************************************************/
static Boolean AppHandleEvent( EventPtr eventP)
{
    Word formId;
    FormPtr frmP;

    if (eventP->eType == frmLoadEvent) {
        // Load the form resource.
        formId = eventP->data.frmLoad.formID;
        frmP = FrmInitForm( formId);
        FrmSetActiveForm( frmP);
        // Set the event handler for the form.  The handler of the currently
        // active form is called by FrmHandleEvent each time is receives an
        // event.
        switch (formId) {
            case GunSmokeForm:
                FrmSetEventHandler(frmP, GunSmokeFormHandleEvent);
                break;
            default:
                ErrNonFatalDisplay("Invalid Form Load Event");
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
 ***********************************************************************/
static void AppEventLoop(void)
{
    Word error;
    EventType event;

    do {
        EvtGetEvent(&event, evtWaitForever);
        if (! SysHandleEvent(&event))
            if (! MenuHandleEvent(0, &event, &error))
                if (! AppHandleEvent(&event))
                    FrmDispatchEvent(&event);
    } while (event.eType != appStopEvent);
}


/***********************************************************************
 *
 * FUNCTION:     AppStart
 *
 * DESCRIPTION:  This routine opens the application's resource file and
 *               database.
 *
 * PARAMETERS:   nothing
 *
 * RETURNED:     Err value 0 if nothing went wrong
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Err AppStart(void)
{
    Word prefsSize;
    Err error;
    
    prefsSize = sizeof(Gs_PreferenceType);
    if (PrefGetAppPreferences(sysFileCAddress, appPrefID, &prefs, &prefsSize, true) != 
                noPreferenceFound) {
        // default prefs
        prefs.target = bullsEye;
    }
    
    MemSet(rounds, sizeof(rounds), 0);
    round_cnt = 0;
    
    error = SysLibFind(MathLibName, &MathLibRef);
    if (error)
        error = SysLibLoad(LibType, MathLibCreator, &MathLibRef);
    ErrFatalDisplayIf(error, "Can't find MathLib"); // Just an example; handle it gracefully
    error = MathLibOpen(MathLibRef, MathLibVersion);
    ErrFatalDisplayIf(error, "Can't open MathLib"); 
    
    draw_screen();

    return 0;
}


/***********************************************************************
 *
 * FUNCTION:    AppStop
 *
 * DESCRIPTION: This routine close the application's database
 *              and save the current state of the application.
 *
 * PARAMETERS:  nothing
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void AppStop(void)
{
    UInt usecount;
    Err error;

	// Write the saved preferences / saved-state information.  This data 
	// will be backed up during a HotSync.
	PrefSetAppPreferences (appFileCreator, appPrefID, appPrefVersionNum, 
		&prefs, sizeof (prefs), true);
		
    error = MathLibClose(MathLibRef, &usecount);
    ErrFatalDisplayIf(error, "Can't close MathLib");
    if (usecount == 0)
        SysLibRemove(MathLibRef); 
    

}


/***********************************************************************
 * FUNCTION:    Gs_PilotMain
 *
 * DESCRIPTION: This is the main entry point for the application.
 *
 * PARAMETERS:  cmd - word value specifying the launch code. 
 *              cmdPB - pointer to a structure that is associated with the 
 *                      launch code. 
 *              launchFlags -  word value providing extra information about 
 *                             the launch.
 *
 * RETURNED:    Result of launch
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static DWord Gs_PilotMain( Word cmd, Ptr cmdPBP, Word launchFlags)
{
    Err error;

    error = RomVersionCompatible(version30, launchFlags);
    if (error) 
        return error;

    switch (cmd) {
        case sysAppLaunchCmdNormalLaunch:
            error = AppStart();
            if (error) 
                return error;
            FrmGotoForm(GunSmokeForm);
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
 *
 * RETURNED:    Result of launch
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
DWord PilotMain( Word cmd, Ptr cmdPBP, Word launchFlags)
{
    return Gs_PilotMain( cmd, cmdPBP, launchFlags);
}
