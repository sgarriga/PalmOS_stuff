/***********************************************************************
 *
 * Copyright (c) 2001 Stephen Garriga
 * All rights reserved.
 *
 **********************************************************************/
#include <Pilot.h>
#include <SysEvtMgr.h>
#include <SoundMgr.h>
#include <ScrDriver.h>
#include <FeatureMgr.h>			//	Needed to detect ROM versions

#include "Dots.h"

/***********************************************************************
 *
 *   Global variables
 *
 ***********************************************************************/
#define SCREEN_X	160
#define SCREEN_Y	160
#define OFFSET_X	10
#define OFFSET_Y	30
#define MENU_BAR    10

#define CELL_X	20
#define CELL_Y	20

#define SQ_LR	(SCREEN_X-OFFSET_X)/CELL_X
#define SQ_TB	(SCREEN_Y-OFFSET_Y)/CELL_Y

typedef struct {
	int score1;
	int score2;
	Boolean gameOver;
	unsigned char player;
	Boolean solo;
	unsigned char matrix[SQ_LR][SQ_TB];
} prefsType;

static prefsType gPref;

#define T_SET	128
#define B_SET	64
#define L_SET	32
#define R_SET	16

#define M_ALL_SET	0xf0
#define M_PLAYER	0x0f

#define P0	0
#define P1	3
#define P2	12

#define SCORE_INCR	10



/***********************************************************************
 *
 *   Internal Constants
 *
 ***********************************************************************/
#define appFileCreator				'SGDz'
#define appVersionNum              	0x01
#define appPrefID                  	0x00
#define appPrefVersionNum          	0x01


// Define the minimum OS version we support
#define ourMinVersion	sysMakeROMVersion(3,1,0,sysROMStageRelease,0)

static RectangleType   viewArea = {{0, 30}, {159, 129}}; 

static CustomPatternType pattern = {0x0000, 0x0000, 0x0000, 0x0000};
static CustomPatternType fill    = {0xffff, 0xffff, 0xffff, 0xffff};

static const RGBColorType black = {0x00, 0x00, 0x00, 0x00};
static const RGBColorType dkGray = {0x00, 0x55, 0x55, 0x55};
static const RGBColorType ltGray = {0x00, 0xAA, 0xAA, 0xAA};
static const RGBColorType white = {0x00, 0xFF, 0xFF, 0xFF};
static const RGBColorType red = {0x00, 0xFF, 0x00, 0x00};
static const RGBColorType blue = {0x00, 0x00, 0x00, 0xFF};

/***********************************************************************
 *
 *   Internal Functions
 *
 ***********************************************************************/


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
			FrmAlert (PalmOSAlert);
		
			// Pilot 1.0 will continuously relaunch this app unless we switch to 
			// another safe one.
			if (romVersion < sysMakeROMVersion(2,0,0,sysROMStageRelease,0))
				AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
			}
		
		return (sysErrRomIncompatible);
		}

	return (0);
}


/***********************************************************************
 *
 * FUNCTION:    Sides
 *
 * DESCRIPTION: This routine counts sides drawn of a cell
 *
 * PARAMETERS:  target cell value
 *
 * RETURNED:    count of sides
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static char Sides(unsigned char target)
{
	char i = 0;
	
	if (target & T_SET)
		i++;
	if (target & B_SET)
		i++;
	if (target & L_SET)
		i++;
	if (target & R_SET)
		i++;
		
	return i;
}

/***********************************************************************
 *
 * FUNCTION:    RandomSide
 *
 * DESCRIPTION: This picks a random side from those available
 *
 * PARAMETERS:  free sides (should be at least 2)
 *
 * RETURNED:    count of sides
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static unsigned char RandomSide(unsigned char target)
{
	unsigned char r = 0;
	int  rnd = SysRandom(0) % Sides(target);
	
	switch (Sides(target))
	{
		case 4:
			r = 1 << rnd;
			break;
			
		case 3:
			r = 1 << rnd;
			if (((r << 4) & target) == 0)
				r = r << 1;				
			break;
			
		case 2:		
			for (r = 1; ((r << 4) & target) == 0; r = r << 1) ;
			
			if (rnd != 0)
				for (r = r << 1; ((r << 4) & target) == 0; r = r << 1) ;

			break;
			
		default:
			r = target;
			break;
	}
	
	return r << 4;
}


/***********************************************************************
 *
 * FUNCTION:    DrawCell
 *
 * DESCRIPTION: This routine draws the set sides of a cell
 *
 * PARAMETERS:  x, y : target cell indices
 *
 * RETURNED:    -
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void DrawCell(int x, int y)
{
	int cx = OFFSET_X + (x * CELL_X);
	int cy = OFFSET_Y + (y * CELL_Y);
	RectangleType r;
	RGBColorType  old;
	
	r.topLeft.x = cx + 2;
	r.topLeft.y = cy + 2;
	r.extent.x = CELL_X - 3;
	r.extent.y = CELL_Y - 3;
	
	if (gPref.matrix[x][y] & T_SET)
		WinDrawLine(cx, cy, cx+CELL_X, cy);
	if (gPref.matrix[x][y] & B_SET)
		WinDrawLine(cx, cy+CELL_Y, cx+CELL_X, cy+CELL_Y);
	if (gPref.matrix[x][y] & L_SET)
		WinDrawLine(cx, cy, cx, cy+CELL_Y);
	if (gPref.matrix[x][y] & R_SET)
		WinDrawLine(cx+CELL_X, cy, cx+CELL_X, cy+CELL_Y);
	
	if ((gPref.matrix[x][y] & M_ALL_SET) == M_ALL_SET)
	{		
		if ((gPref.matrix[x][y] & M_PLAYER) == P1)
			WinSetColors(&dkGray, &old, 0, 0); // WinSetForeColor
		else if ((gPref.matrix[x][y] & M_PLAYER) == P2)
			WinSetColors(&ltGray, &old, 0, 0); // WinSetForeColor			
			
		WinSetPattern(fill);
		WinFillRectangle(&r, 0);
		
		WinSetColors(&old, NULL, 0, 0);
	}
}


/***********************************************************************
 *
 * FUNCTION:    SetSide
 *
 * DESCRIPTION: This routine sets the flag for a cells side (and its compliment)
 *
 * PARAMETERS:  x, y : target cell indices
 *              side : side flag
 *
 * RETURNED:    true = cell completed
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static unsigned char SetSide(int x, int y, unsigned char side)
{
	unsigned char retval = false;
	
	gPref.matrix[x][y] |= side;
	if ((gPref.matrix[x][y] & M_ALL_SET) == M_ALL_SET)
	{
		gPref.matrix[x][y] |= gPref.player;
		retval = true;
	}
	DrawCell(x, y);
	
	if      ((side == T_SET) && (y > 0))
	// not top row, so set bottom of row above
	{
		gPref.matrix[x][y-1] |= B_SET;
		if (((gPref.matrix[x][y-1] & M_ALL_SET) == M_ALL_SET) && 
			((gPref.matrix[x][y-1] & M_PLAYER) == P0))
		{
			gPref.matrix[x][y-1] |= gPref.player;
			retval = true;
			DrawCell(x, y-1);
		}
	}
	else if ((side == B_SET) && (y < SQ_TB-1))
	// not bottom row, so set top of row below
	{
		gPref.matrix[x][y+1] |= T_SET;
		if (((gPref.matrix[x][y+1] & M_ALL_SET) == M_ALL_SET) && 
			((gPref.matrix[x][y+1] & M_PLAYER) == P0))
		{
			gPref.matrix[x][y+1] |= gPref.player;
			retval = true;
			DrawCell(x, y+1);
		}
	}
	else if ((side == L_SET) && (x > 0))
	// not left edge, so set right of row before
	{
		gPref.matrix[x-1][y] |= R_SET;
		if (((gPref.matrix[x-1][y] & M_ALL_SET) == M_ALL_SET) && 
			((gPref.matrix[x-1][y] & M_PLAYER) == P0))
		{
			gPref.matrix[x-1][y] |= gPref.player;
			retval = true;
			DrawCell(x-1, y);
		}
	}
	else if ((side == R_SET) && (x < SQ_LR-1))
	// not right edge, so set left of row after
	{
		gPref.matrix[x+1][y] |= L_SET;
		if (((gPref.matrix[x+1][y] & M_ALL_SET) == M_ALL_SET) && 
			((gPref.matrix[x+1][y] & M_PLAYER) == P0))
		{
			gPref.matrix[x+1][y] |= gPref.player;
			retval = true;
			DrawCell(x+1, y);
		}
	}
	
	return retval;
}


/***********************************************************************
 *
 * FUNCTION:    CheckTurn
 *
 * DESCRIPTION: This routine determines selected edge.
 *
 * PARAMETERS:  
 *              x, y  : indices for screen (in)
 *              X, Y  : indices for matrix (out)
 *              side  : side changed (out)
 *
 * RETURNED:    true = valid, false = invalid
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static char CheckTurn(int scrX, int scrY, int *x, int *y, unsigned char *side)
{
	// position in cell
	int cx = (scrX-OFFSET_X) % CELL_X;
	int cy = (scrY-OFFSET_Y) % CELL_Y;
	
	*side = 0;
	
	// select cell
	*x = (scrX-OFFSET_X) / CELL_X;
	*y = (scrY-OFFSET_Y) / CELL_Y;
	    
	if ((*x >= SQ_LR) || (*y >= SQ_TB) || (cx >= CELL_X) || (cy >= CELL_Y))
	    return false;

	// check if tab is close to an edge
	// (using 1/3 for moment)
	if ((cy < CELL_Y/3) && // Tmb
	    (cx > CELL_X/3) && (cx <= 2*CELL_X/3)) // lMr
		*side = T_SET;
	else
	if ((cy > 2*CELL_Y/3) && // tmB
	    (cx > CELL_X/3) && (cx <= 2*CELL_X/3)) // lMr
		*side = B_SET;
	else
	if ((cx < CELL_X/3) && // Lmr
	    (cy > CELL_Y/3) && (cy <= 2*CELL_Y/3)) // tMb
		*side = L_SET;
	else
	if ((cx > 2*CELL_X/3) && // lmR
	    (cy > CELL_Y/3) && (cy <= 2*CELL_Y/3)) // tMb
		*side = R_SET;
	else
		return false;
		
	if ((gPref.matrix[*x][*y] & *side) == *side) // already set
		return false;
		
	return true;
}


/***********************************************************************
 *
 * FUNCTION:    ValidTurn
 *
 * DESCRIPTION: This routine updates the screen and score following a 
 *              valid turn.
 *
 * PARAMETERS:  x, y indices for matrix, side set
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void ValidTurn(int x, int y, unsigned char side)
{
	char banner[80];
	int freeSq;
	unsigned char closure, oneClosed;
	
	oneClosed = closure = SetSide(x, y, side);
	
	SndPlaySystemSound (sndClick);
	
	// do the freebies ...
	while (closure)
	{
		closure = false;
		for (x=0; x<SQ_LR; x++)
			for (y=0; y<SQ_TB; y++)
				// if we just closed a square we get a free line ....
				if (Sides(gPref.matrix[x][y]) == 3)
				{				
					closure = SetSide(x, y, (gPref.matrix[x][y] ^ M_ALL_SET) & M_ALL_SET);

					SndPlaySystemSound (sndClick);
				}
	}
			
	// Increase score for each square
	gPref.score1 = gPref.score2 = 0;
	freeSq = 0;
	for (x=0; x<SQ_LR; x++)
		for (y=0; y<SQ_TB; y++)
		{				
			if ((gPref.matrix[x][y] & M_ALL_SET) == M_ALL_SET)
			{					
				if ((gPref.matrix[x][y] & M_PLAYER) == P1)
					gPref.score1+=SCORE_INCR;
				else
					gPref.score2+=SCORE_INCR;
			}
			else
				freeSq++;
		}

	if (!oneClosed)
		if (gPref.player == P1)
			gPref.player = P2;
		else
			gPref.player = P1;
			
	if (freeSq > 0)
	{
		StrPrintF(banner, 
		          "Player 1: %04d   Player 2: %04d   P%d Up", 
		          gPref.score1, 
		          gPref.score2,
		          (gPref.player == P1)?1:2);
	}
	else
	{
		if (gPref.score1 == gPref.score2)
			StrPrintF(banner, "Players Draw                                                ");
		else
			StrPrintF(banner, 
			          "Congratulations.... Player %d wins               ", 
			          (gPref.score1 > gPref.score2)?1:2);
		gPref.gameOver = true;
	}
	WinDrawChars(banner,StrLen(banner),5,12);	
}

/***********************************************************************
 *
 * FUNCTION:    ComputerTurn
 *
 * DESCRIPTION: Generate a computer turn
 *
 * PARAMETERS:  -
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void ComputerTurn(void)
{
	int x, y;
	unsigned char side = 0;
	
	// any 3 sided?
	for (x=0; x<SQ_LR; x++)
		for (y=0; y<SQ_TB; y++)
			if (Sides(gPref.matrix[x][y]) == 3)
			{
				side = (gPref.matrix[x][y] ^ M_ALL_SET) & M_ALL_SET;
				ValidTurn(x, y, side);
				return; 				
			}

	// any 0 sided			
	for (x=0; x<SQ_LR; x++)
		for (y=0; y<SQ_TB; y++)
			if ((gPref.matrix[x][y] & M_ALL_SET) == 0)
			{
				side = RandomSide(M_ALL_SET);
				ValidTurn(x, y, side);
				return; 				
			}
			
	// any 1 sided			
	for (x=0; x<SQ_LR; x++)
		for (y=0; y<SQ_TB; y++)
			if (Sides(gPref.matrix[x][y]) == 1)
			{
				side = RandomSide(gPref.matrix[x][y] ^ M_ALL_SET);
				ValidTurn(x, y, side);
				return; 				
			}

	// any 2 sided			
	for (x=0; x<SQ_LR; x++)
		for (y=0; y<SQ_TB; y++)
			if ((gPref.matrix[x][y] & M_ALL_SET) != M_ALL_SET)
			{
				side = RandomSide(gPref.matrix[x][y] ^ M_ALL_SET);
				ValidTurn(x, y, side);
				return; 				
			}

			
}



/***********************************************************************
 *
 * FUNCTION:    InalidTurn
 *
 * DESCRIPTION: This routine notifies the player the turn is not done
 *
 * PARAMETERS:  -
 *
 * RETURNED:    nothing
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void InvalidTurn()
{
	SndPlaySystemSound (sndError);
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
	FormPtr	    prefFormP = FrmInitForm( OptionsForm );
	ControlPtr  chkSolo   = FrmGetObjectPtr(prefFormP, 
                                     FrmGetObjectIndex(prefFormP, 
                                         OptionsOnePlayerCheckbox));
			
	// Put up the form
	buttonHit = FrmDoDialog( prefFormP );
	
	// Collect new setting
	if ( buttonHit == OptionsOKButton )
	{
		gPref.solo = CtlGetValue(chkSolo);	
	}
	// Delete the form
	FrmDeleteForm( prefFormP );
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
 ***********************************************************************/
static void MainFormInit(FormPtr frmP)
{
	int x, y, cx, cy;
	char banner[80];

	StrPrintF(banner, 
	          "Player 1: %04d   Player 2: %04d   P%d Up", 
	          gPref.score1, 
	          gPref.score2,
	          (gPref.player == P1)?1:2);
	WinDrawChars(banner,StrLen(banner),5,12);
	for (cx = OFFSET_X; cx < SCREEN_X; cx+=CELL_X)
		for (cy = OFFSET_Y; cy < SCREEN_Y; cy+=CELL_Y)
		{
			WinDrawLine(cx-1, cy-1, cx+1, cy-1);
			WinDrawLine(cx-1, cy-1, cx-1, cy+1);
			WinDrawLine(cx, cy, cx, cy);
			WinDrawLine(cx+1, cy-1, cx+1, cy+1);
			WinDrawLine(cx-1, cy+1, cx+1, cy+1);
		}
		
	// just in case we saved game ....
	for (x=0; x<SQ_LR; x++)
		for (y=0; y<SQ_TB; y++)
			DrawCell(x, y);
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
 ***********************************************************************/
static Boolean MainFormHandleEvent(EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;
    Handle  verH, namH;
	CharPtr ver, nam;
	int 	x, y;
	unsigned char side;

	switch (eventP->eType) 
	{
		case menuEvent:
			switch (eventP->data.menu.itemID)
			{
				case AboutAbout:
					MenuEraseStatus (0);  
				    namH = DmGet1Resource('tAIN', 1000);
				    verH = DmGet1Resource('tver', 1000);
				    nam = MemHandleLock(namH);
			    	ver = MemHandleLock(verH);
					FrmCustomAlert(AboutAlert, nam, ver, NULL);
				    MemHandleUnlock(namH);
				    MemHandleUnlock(verH);
				    DmReleaseResource(namH);
				    DmReleaseResource(verH);
					handled = true;
					break;
					
				case AboutOptions:
					ShowOptions();
					// DO NOT BREAK
				case AboutNewGame:			
			    	gPref.score1 = 0;
					gPref.score2 = 0;

					for (x=0; x<SQ_LR; x++)
						for (y=0; y<SQ_TB; y++)
							gPref.matrix[x][y] = 0;
							
					WinSetPattern(pattern);
    				WinFillRectangle(&viewArea, 0);
    				
    				gPref.gameOver = false;
    
			    	frmP = FrmGetActiveForm();
					MainFormInit(frmP);
					handled = true;
					break;
				
				default:
					break;
			}
			break;

		case frmOpenEvent:
			frmP = FrmGetActiveForm();
			FrmDrawForm (frmP);
			MainFormInit(frmP);
			handled = true;
			break;
			
		case penDownEvent:
			if (!gPref.gameOver && (eventP->screenY > MENU_BAR))
			{
				if (CheckTurn(eventP->screenX, eventP->screenY, &x, &y, &side))
				{
					ValidTurn(x, y, side);
					if (gPref.solo)
						while (gPref.player == P2)
							ComputerTurn();
				}
				else
					InvalidTurn();
				handled = true;
			}
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
 ***********************************************************************/
static void AppEventLoop(void)
{
	Word error;
	EventType event;

	do 
	{
		EvtGetEvent(&event, evtWaitForever);
			
		if (! SysHandleEvent(&event))
			if (! MenuHandleEvent(0, &event, &error))
				if (! AppHandleEvent(&event))
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
    Word prefsSize;
    int x, y;
    DWord	requiredDepth = 2;      // Apps needing B/W should use 1, grayscale apps use 2
    
    ScrDisplayMode(scrDisplayModeSet, NULL, NULL, &requiredDepth, NULL);
       
    prefsSize = sizeof(gPref);
    if (PrefGetAppPreferences(sysFileCAddress, 
                              appPrefID, 
                              &gPref, 
                              &prefsSize, 
                              true) == noPreferenceFound) 
    {
		for (x=0; x<SQ_LR; x++)
			for (y=0; y<SQ_TB; y++)
				gPref.matrix[x][y] = 0;
	   	gPref.score1 = 0;
		gPref.score2 = 0;
		gPref.player = P1;
		gPref.solo = true;
		gPref.gameOver = true;
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
	PrefSetAppPreferences (appFileCreator,
						   appPrefID, 
						   appPrefVersionNum, 
						   &gPref,
						   sizeof(gPref), 
						   true);
}


/***********************************************************************
 *
 * FUNCTION:    DotsPilotMain
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
 ***********************************************************************/
static DWord DotsPilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
	Err error;

	error = RomVersionCompatible (ourMinVersion, launchFlags);
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
 ***********************************************************************/
DWord PilotMain( Word cmd, Ptr cmdPBP, Word launchFlags)
{
    return DotsPilotMain(cmd, cmdPBP, launchFlags);
}

