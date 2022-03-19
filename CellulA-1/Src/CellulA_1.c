#include <Pilot.h>
#include <SysEvtMgr.h>
#include <SysUtils.h>
#include <StringMgr.h>
#include <FeatureMgr.h>			//	Needed to detect ROM versions
#include <ScrDriver.h>
#include <Window.h>
#include <Rect.h>
#include <DateTime.h>
#include "CellulA_1Rsc.h"

/***********************************************************************
 *   Entry Points
 ***********************************************************************/

static Boolean RulesFormHandleEvent( EventPtr eventP);
static Boolean SeedFormHandleEvent( EventPtr eventP);
static void SetSeed(void);

/***********************************************************************
 * Local Defines
 ***********************************************************************/
#define SCREEN_WIDTH    160
#define START_ROW       20
#define INTERVAL        2

// 1 generation per pixel row
#define MAX_GENERATIONS 110

// rule 8->1 = 00011110
#define DEFAULT_RULE    0x1e

typedef enum seed { seedRandom, seedAlternate, seedCenter } seed_t;
typedef enum mutate { mutRandom, mutEvery8, mutNone } mutate_t;

/***********************************************************************
 *   Internal Structures
 ***********************************************************************/
typedef struct { Byte       rule;           //  8 rule results
                 seed_t     seed;           //  seeding type
                 mutate_t   mutate;         //  mutation rule
               } My_PreferenceType;

/***********************************************************************
 *   Global variables
 ***********************************************************************/
static My_PreferenceType prefs;
static Boolean HideSecretRecords;
static DmOpenRef My_DB;
static FontID AppFontID = stdFont; 
static Byte running = 0;
static Boolean row_old[SCREEN_WIDTH];
static Boolean row_new[SCREEN_WIDTH];
static int two_power[8] = {1, 2, 4, 8, 16, 32, 64, 128};
static RectangleType main_rect;

/***********************************************************************
 *   Internal Constants
 ***********************************************************************/
//TODO Change the file creator ID'
#define appFileCreator             'SGC1'
#define appVersionNum              0x01
#define appPrefID                  0x00
#define appPrefVersionNum          0x03
#define appDBName                  "My_DB"
#define appDBType                  'DATA'
// Version Checking define
#define version30                  0x03000000
#define version20                  0x02000000

/***********************************************************************
 *   Internal Functions
 ***********************************************************************/

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
 *                                (see sysFtrNumROMVersion in SystemMgr.h 
 *                                for format)
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
    if (romVersion < requiredVersion) 
    {
        if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
            (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) 
        {
            FrmAlert(ROMVersionAlert);
            // Pilot 1.0 will continuously relaunch this app unless we switch to 
            // another safe one.
            if (romVersion < version20) 
            {
                AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
            }
        }
        return sysErrRomIncompatible;
    }
    return 0;
}

/***********************************************************************
 * FUNCTION:    StartSimulation
 *
 * DESCRIPTION: This procedure generates the 1st (seed) generation
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void StartSimulation()
{
    short i;

    WinEraseRectangle(&main_rect, 0);
    
    SetSeed();    

    for (i=0; i<SCREEN_WIDTH; i++)
    {
        if (row_old[i])
            WinDrawLine(i, START_ROW, i, START_ROW);
    }
}

/***********************************************************************
 * FUNCTION:    DoSimulation
 *
 * DESCRIPTION: This procedure generates the 2nd - Nth generation
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void DoSimulation()
{
    short i;
    Byte pattern;
    static short gens    = 1;
    static short gensCnt = 8;

    if (prefs.mutate == mutEvery8)
    {
        gens++;
        if (gens == gensCnt)
        {
            prefs.rule = (SysRandom(0) % 256);
            gensCnt = 8;
            gens = 0;
        }
    }
    else if (prefs.mutate == mutRandom)
    {
        if ((SysRandom(0) % 256) == 128)
        {
            prefs.rule = (SysRandom(0) % 256);
        }
    }


    for (i=0; i<SCREEN_WIDTH; i++)
    {
        pattern = 0;
        if ((i > 0) && (row_old[i-1])) 
            pattern += 4;
        if (row_old[i])   
            pattern += 2;
        if ((i < SCREEN_WIDTH-1) && (row_old[i+1])) 
            pattern += 1;
        
        row_new[i] = ((two_power[pattern] & prefs.rule) != 0);
        if (row_new[i])
            WinDrawLine(i, START_ROW+running, i, START_ROW+running);
    }

    for (i=0; i<SCREEN_WIDTH; i++)
        row_old[i] = row_new[i];
                
}

/***********************************************************************
 * FUNCTION:    RulesFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the
 *              "RulesForm" of this application.
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Boolean RulesFormHandleEvent( EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;

    switch (eventP->eType) 
    {
        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) 
            {
                case RulesSaveButton:
                    break;

                case RulesDefaultButton:
                    prefs.rule = DEFAULT_RULE;
                    break;

                default:
                    ErrNonFatalDisplay("Unexpected Event");
                    break;
            }
            break;

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
 * FUNCTION:    ChangeRules
 *
 * DESCRIPTION: This procedure handles the RULES changes screen
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void ChangeRules(void)
{
	Word		buttonHit = 0;
	FormPtr	    prefFormP;
	Word		controlID[8];
	Byte        new_rule = 0;
	
    // Create the form
	prefFormP = FrmInitForm( RulesForm );
	
	while (buttonHit != RulesSaveButton)
    {		
		// Initialize Rules
	    if (prefs.rule & 1) 
	        controlID[0] = RulesLive0PushButton; 
	    else 
	        controlID[0] = RulesDie0PushButton;
	    if (prefs.rule & 2) 
	        controlID[1] = RulesLive1PushButton; 
	    else 
	        controlID[1] = RulesDie1PushButton;
	    if (prefs.rule & 4) 
	        controlID[2] = RulesLive2PushButton; 
	    else 
	        controlID[2] = RulesDie2PushButton;
	    if (prefs.rule & 8) 
	        controlID[3] = RulesLive3PushButton; 
	    else 
	        controlID[3] = RulesDie3PushButton;
	    if (prefs.rule & 16) 
	        controlID[4] = RulesLive4PushButton; 
	    else 
	        controlID[4] = RulesDie4PushButton;
	    if (prefs.rule & 32) 
	        controlID[5] = RulesLive5PushButton; 
	    else 
	        controlID[5] = RulesDie5PushButton;
	    if (prefs.rule & 64) 
	        controlID[6] = RulesLive6PushButton; 
	    else  
	        controlID[6] = RulesDie6PushButton;
	    if (prefs.rule & 128) 
	        controlID[7] = RulesLive7PushButton; 
	    else 
	        controlID[7] = RulesDie7PushButton;
	        
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID,  controlID[0] );
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID2, controlID[1] );
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID3, controlID[2] );
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID4, controlID[3] );
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID5, controlID[4] );
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID6, controlID[5] );
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID7, controlID[6] );
		FrmSetControlGroupSelection ( prefFormP, RulesFormGroupID8, controlID[7] );
	        	
		// Put up the form
		buttonHit = FrmDoDialog( prefFormP );
		
		// Collect new setting
		switch (buttonHit)
		{
		    case RulesSaveButton:
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 
		                                                        RulesFormGroupID)) == 
		              RulesLive0PushButton)
		            new_rule |= 1;
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 	         
	                                                            RulesFormGroupID2)) == 
		               RulesLive1PushButton)
		            new_rule |= 2;
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 
		                                                        RulesFormGroupID3)) == 
		              RulesLive2PushButton)
		            new_rule |= 4;
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 
		                                                        RulesFormGroupID4)) == 
		              RulesLive3PushButton)
		            new_rule |= 8;
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 
		                                                        RulesFormGroupID5)) == 
		              RulesLive4PushButton)
		            new_rule = new_rule | 16;
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 
		                                                        RulesFormGroupID6)) == 
		              RulesLive5PushButton)
		            new_rule |= 32;
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 
		                                                        RulesFormGroupID7)) == 
		              RulesLive6PushButton)
		            new_rule |= 64;
		        if (FrmGetObjectId( prefFormP, 
		                            FrmGetControlGroupSelection(prefFormP, 
		                                                        RulesFormGroupID8)) == 
		              RulesLive7PushButton)
		            new_rule |= 128;
		            
		        prefs.rule = new_rule;
		        continue;
	   	        
		    case RulesRandomButton:
    		    prefs.rule = (SysRandom(0) % 256);
		        break;
		        
	        case RulesDefaultButton:
		        prefs.rule = DEFAULT_RULE;
		        break;
		        
   	        default: // exit on unknown
   		        buttonHit = RulesSaveButton;
   		        continue;
		}			
		
        FrmDrawForm( prefFormP );               	

	}	
				
    // Delete the form
  	FrmDeleteForm( prefFormP );

}

/***********************************************************************
 * FUNCTION:    SetSeed
 *
 * DESCRIPTION: This procedure initialises the seed array based on seed
 *              type selected.
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void SetSeed(void)
{
    short i;
    switch (prefs.seed) 
    {
        case seedRandom:
            // set 1 in 32 chance
            for (i=0; i<SCREEN_WIDTH; i++)
            {
                row_old[i] = ((SysRandom(0)%32) == 11);
            }
            break;
            
        case seedAlternate:
            for (i=0; i<SCREEN_WIDTH; i+=2)
            {
                row_old[i] = false; 
                row_old[i+1] = true; 
            }
            break;
            
        case seedCenter:
        default:
            for (i=0; i<SCREEN_WIDTH; i++)
                row_old[i] = false;        

            row_old[SCREEN_WIDTH/2] = true;          
    }
}

/***********************************************************************
 * FUNCTION:    ChangeSeed
 *
 * DESCRIPTION: This procedure handles the SEED change screen
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void ChangeSeed(void)
{
	Word		buttonHit;
	FormPtr	prefFormP;
	Word		controlID;
	
	// Create the form
	prefFormP = FrmInitForm( SeedForm );
		
	// Initialize difficulty level selection
	switch( prefs.seed )
	{
		case seedRandom:
			controlID = SeedRB_randomPushButton;
			break;

		case seedAlternate:
			controlID = SeedRB_alternatePushButton; 
			break;

		case seedCenter:
			controlID = SeedRB_centerPushButton;
			break;

		default:
			controlID = SeedRB_centerPushButton;
			break;
	}
	FrmSetControlGroupSelection ( prefFormP, SeedFormGroupID, controlID );
	
	// Put up the form
	buttonHit = FrmDoDialog( prefFormP );
	
	// Collect new setting
	if ( buttonHit == SeedSaveButton )
	{
		controlID = FrmGetObjectId( prefFormP,
				FrmGetControlGroupSelection(prefFormP, SeedFormGroupID) );
		switch ( controlID )
		{
			case SeedRB_randomPushButton:
				prefs.seed = seedRandom;
				break;
				
			case SeedRB_alternatePushButton:
				prefs.seed = seedAlternate;
				break;
				
			case SeedRB_centerPushButton:
				prefs.seed = seedCenter;
				break;
		}

        SetSeed();
	}
	
	// Delete the form
	FrmDeleteForm( prefFormP );
}

/***********************************************************************
 * FUNCTION:    ChangeEvolution
 *
 * DESCRIPTION: This procedure handles the EVOLUTION change screen
 *
 * PARAMETERS:  none
 *
 * RETURNED:    none
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static void ChangeEvolution(void)
{
	Word		buttonHit;
	FormPtr	    prefFormP;
	Word		controlID;
	
	// Create the form
	prefFormP = FrmInitForm( EvolutionForm );
		
	switch( prefs.mutate )
	{
		case mutEvery8:
			controlID = EvolutionEvery8PushButton;
			break;

		case mutRandom:
			controlID = EvolutionRandomPushButton;
			break;

		case mutNone:
		default:
			controlID = EvolutionNonePushButton;
			break;
	}
	FrmSetControlGroupSelection ( prefFormP, EvolutionFormGroupID, controlID );
	
	// Put up the form
	buttonHit = FrmDoDialog( prefFormP );
	
	// Collect new setting
	if ( buttonHit == EvolutionSaveButton )
	{
		controlID = FrmGetObjectId( prefFormP,
				FrmGetControlGroupSelection(prefFormP, EvolutionFormGroupID) );
		switch ( controlID )
		{
			case EvolutionRandomPushButton:
				prefs.mutate = mutRandom;
				break;
				
			case EvolutionEvery8PushButton:
				prefs.mutate = mutEvery8;
				break;
				
			case EvolutionNonePushButton:
				prefs.mutate = mutNone;
				break;
		}
	}
	
	// Delete the form
	FrmDeleteForm( prefFormP );
}


/***********************************************************************
 * FUNCTION:    ShowAbout
 *
 * DESCRIPTION: This procedure handles the About ... info
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
	Word		buttonHit;
	FormPtr	prefFormP;
	
	prefFormP = FrmInitForm( AboutForm );
		
	buttonHit = FrmDoDialog( prefFormP );
	
	FrmDeleteForm( prefFormP );
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
 ***********************************************************************/
static Boolean MainFormDoCommand(Word command)
{
    Boolean handled = false;
  
    switch (command) 
    {
        case OptionsRules: 
            ChangeRules();
            handled = true;
            break;

        case OptionsSeed:
            ChangeSeed();
            handled = true;
            break;

        case OptionsEvolution:
            ChangeEvolution();
            handled = true;
            break;
        
        case OptionsAbout:
            ShowAbout();
            handled = true;
            break;
            
        default:
            ErrNonFatalDisplay("Unexpected Event");
            break;

    }

    return handled;
}

/***********************************************************************
 * FUNCTION:    MainFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the
 *              "MainForm" of this application.
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Boolean MainFormHandleEvent( EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;


    switch (eventP->eType) 
    {
        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) 
            {
                case MainRunButton:
                    running = 1;
                    StartSimulation();
                    handled = true;
                    break;

                case MainStopButton:
                    running = MAX_GENERATIONS;
                    handled = true;
                    break;

                default:
                    ErrNonFatalDisplay("Unexpected Event");
                    break;
            }
            break;
        
        case nilEvent:
            if ((running > 0) && (running < MAX_GENERATIONS))
            {
                DoSimulation();
                running++;
            }
            handled = true;
            break;
            
        case menuEvent:
            handled = MainFormDoCommand(eventP->data.menu.itemID);
            break;

        case frmOpenEvent:
            frmP = FrmGetActiveForm();
            WinDrawGrayRectangleFrame(simpleFrame, &main_rect);
            FrmDrawForm ( frmP);
            handled = true;
            break;

        default:
            break;
    }

    return handled;
}

/***********************************************************************
 * FUNCTION:    SeedFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the
 *              "SeedForm" of this application.
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Boolean SeedFormHandleEvent( EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;


    switch (eventP->eType) 
    {
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
 * FUNCTION:    AboutFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the
 *              "AboutForm" of this application.
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event has handle and should not be passed
 *              to a higher level handler.
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Boolean AboutFormHandleEvent( EventPtr eventP)
{
    Boolean handled = false;
    FormPtr frmP;

    switch (eventP->eType) 
    {
        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) 
            {
                case AboutOKButton:
                    handled = true;
                    break;

                default:
                    ErrNonFatalDisplay("Unexpected Event");
                    break;
            }
            break;

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

    if (eventP->eType == frmLoadEvent) 
    {
        // Load the form resource.
        formId = eventP->data.frmLoad.formID;
        frmP = FrmInitForm( formId);
        FrmSetActiveForm( frmP);
        // Set the event handler for the form.  The handler of the currently
        // active form is called by FrmHandleEvent each time is receives an
        // event.
        switch (formId) 
        { 
            case RulesForm:
                FrmSetEventHandler(frmP, RulesFormHandleEvent);
                break;
            case MainForm:
                FrmSetEventHandler(frmP, MainFormHandleEvent);
                break;
            case SeedForm:
                FrmSetEventHandler(frmP, SeedFormHandleEvent);
                break;
            case AboutForm:
                FrmSetEventHandler(frmP, AboutFormHandleEvent);
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

    do 
    {
        EvtGetEvent(&event, INTERVAL);
        
        // suppress timeouts when menu pressed
        if (event.eType == keyDownEvent)
            if(event.data.keyDown.chr == menuChr)
                running = MAX_GENERATIONS;
        

        if (! SysHandleEvent(&event))
        {
            if (! MenuHandleEvent(0, &event, &error))
            {
                if (! AppHandleEvent(&event))
                {
                    FrmDispatchEvent(&event);                           
                }
            }

        }        
    } while (event.eType != appStopEvent);
}

/***********************************************************************
 *
 * FUNCTION:     AppStart
 *
 * DESCRIPTION:  This routine gets the application's prefs.
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
    
	// Read the saved preferences / saved-state information.
	prefsSize = sizeof(My_PreferenceType);
	if (PrefGetAppPreferences(appFileCreator, appPrefID, &prefs, &prefsSize, true) == 
		noPreferenceFound)
	{
		prefs.rule = DEFAULT_RULE;
        prefs.seed = seedCenter;
        prefs.mutate = mutNone;
	}

    SetSeed();
    
	main_rect.topLeft.x = 0;
	main_rect.topLeft.y = START_ROW;
	main_rect.extent.x  = SCREEN_WIDTH;
	main_rect.extent.y  = MAX_GENERATIONS;
	
	// seed random number generator
	(void) SysRandom(TimGetTicks());
	
    return 0;
}

/***********************************************************************
 *
 * FUNCTION:    AppStop
 *
 * DESCRIPTION: This routine close the application's prefs
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
	// Write the saved preferences / saved-state information.  This data 
	// will be backed up during a HotSync.
	PrefSetAppPreferences (appFileCreator, appPrefID, appPrefVersionNum, 
		&prefs, sizeof (prefs), true);
}

/***********************************************************************
 * FUNCTION:    My_PilotMain
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
static DWord My_PilotMain( Word cmd, Ptr cmdPBP, Word launchFlags)
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
    return My_PilotMain(cmd, cmdPBP, launchFlags);
}
