/******************************************************************************
 *
 * Copyright 2001 Stephen Garriga
 * All rights reserved.
 *
 * File: Dotz.c
 *
 *****************************************************************************/

#include <PalmOS.h>
#include <Form.h>
#include "DotzRsc.h"

#define SCREEN_X	160
#define SCREEN_Y	160
#define OFFSET_X	1
#define OFFSET_Y	30
#define MENU_BAR    10

// enums for flags
typedef enum player {one_player = 128, two_player = 0} player_t;

// enum for game state
typedef enum who_up {player_one_up = 64, player_two_up = 0} who_up_t;

typedef enum colors {color_mask = 56,
                     r_g = 56,
                     r_b = 48,
                     g_r = 40,
                     g_b = 32,
                     b_r = 24,
                     b_g = 16,
                     d_l = 8,
                     l_d = 0} colors_t;
typedef enum scrn_sz {szMask = 6,
                      szScr1 = 6,
                      szScr2 = 4,
                      szScr3 = 2,
                      szScr4 = 0} screen_size_t;
typedef enum palate {monochrome = 1, color = 0} palate_t;

// enums for square state
typedef enum side {LRTB_taken = 15,
                   L_taken = 8,
                   R_taken = 4,
                   T_taken = 2,
                   B_taken = 1} side_taken_t;
typedef enum who_got {player_one_got = 16, player_two_got = 32} who_got_t;              

#define MAX_X   12
#define MAX_Y   12

#define CELL_X ((SCREEN_X-OFFSET_X)/sizeX())
#define CELL_Y ((SCREEN_Y-OFFSET_Y)/sizeY())


typedef struct {
    char           name[80];
    unsigned short points;
} hiEntry_t;

typedef struct {
    unsigned char  board[MAX_X][MAX_Y];
    hiEntry_t      score[10];
    unsigned char  flags;
} prefs_t;

static RGBColorType black = {0x00, 0x00, 0x00, 0x00};
static RGBColorType dkGray = {0x00, 0x55, 0x55, 0x55};
static RGBColorType ltGray = {0x00, 0xAA, 0xAA, 0xAA};
static RGBColorType white = {0x00, 0xFF, 0xFF, 0xFF};
static RGBColorType red = {0x00, 0xFF, 0x00, 0x00};
static RGBColorType blue = {0x00, 0x00, 0x00, 0xFF};
static RGBColorType green = {0x00, 0x00, 0xFF, 0x00};

// *****************************************************************

static prefs_t prefs;

static UInt32 g_color_bits = 0;

static RGBColorType *ColPlayer[2];

#define appFileCreator             'SGDz'
#define appVersionNum              0x20
#define appPrefID                  0x00
#define appPrefVersionNum          0x01

// Define the minimum OS version we support (3.5 for now).
#define ourMinVersion    sysMakeROMVersion(3,5,0,sysROMStageRelease,0)


/***********************************************************************
 *
 *   Internal Functions
 *
 ***********************************************************************/
 
// Set colors from prefs
static void setColors()
{
    switch (prefs.flags & color_mask)
    {
        case r_g :
            ColPlayer[0] = &red;
            ColPlayer[1] = &green;
            break;
        case r_b :
            ColPlayer[0] = &red;
            ColPlayer[1] = &blue;
            break;
        case g_r :
            ColPlayer[0] = &green;
            ColPlayer[1] = &red;
            break;
        case g_b :
            ColPlayer[0] = &green;
            ColPlayer[1] = &blue;
            break;
        case b_r :
            ColPlayer[0] = &blue;
            ColPlayer[1] = &red;
            break;
        case b_g :
            ColPlayer[0] = &blue;
            ColPlayer[1] = &green;
            break;
        case d_l :
            ColPlayer[0] = &dkGray;
            ColPlayer[1] = &ltGray;
            break;
        case l_d :
            ColPlayer[0] = &ltGray;
            ColPlayer[1] = &dkGray;
            break;
    }
}
         
// Number of columns
static unsigned char sizeX()
{
    switch (prefs.flags & (unsigned char) szMask)
    {
        case szScr1 : return MAX_X;   break;
        case szScr2 : return 2*MAX_X/3; break;
        case szScr3 : return MAX_X/2; break;
        case szScr4 : return MAX_X/3; break;
        default : return -1;
    }
}

// Number of rows
static unsigned char sizeY()
{
    switch (prefs.flags & (unsigned char) szMask)
    {
        case szScr1 : return MAX_Y;   break;
        case szScr2 : return 2*MAX_Y/3; break;
        case szScr3 : return MAX_Y/2; break;
        case szScr4 : return MAX_Y/3; break;
        default : return -1;
    }
}

// Number of moves left
static unsigned char turns_left()
{
    unsigned char x, y;
    unsigned char free_squares = 0;

    for (x = 0; x < sizeX(); x++)
        for (y = 0; y < sizeY(); y++)
            if ((prefs.board[x][y] & (unsigned char) LRTB_taken) != 
                       LRTB_taken)
                free_squares++;

    return free_squares;
}

// Number of untaken sides on a square
static unsigned char sidesFree(unsigned char square)
{
    unsigned char sf = 0;
    if ((square & (unsigned char) LRTB_taken) == LRTB_taken)
        return sf;
        
    if ((square & (unsigned char) L_taken) != L_taken)
        sf++;
        
    if ((square & (unsigned char) T_taken) != T_taken)
        sf++;
        
    if ((square & (unsigned char) R_taken) != R_taken)
        sf++;
        
    if ((square & (unsigned char) B_taken) != B_taken)
        sf++;
        
    return sf;
}

// notifies the player the turn is not done
static void InvalidTurn()
{
	SndPlaySystemSound (sndError);
}

//notifies the player they go again
static void P0InvalidTurn()
{
	SndPlaySystemSound (sndInfo);
}

//notifies the next player they go
static void ValidTurn()
{
	SndPlaySystemSound (sndConfirmation);
}


// set the flags for a side being validly selected
static void setSide(unsigned char x, unsigned char y, 
                    unsigned char side, unsigned char who)
{
     prefs.board[x][y] |= side;
     
     if ((x > 0) && (side == L_taken))
         prefs.board[x-1][y] |= R_taken;
         
     if ((y > 0) && (side == T_taken))
         prefs.board[x][y-1] |= B_taken;
     
     if ((x < sizeX()) && (side == R_taken))
         prefs.board[x+1][y] |= L_taken;
         
     if ((y < sizeY()) && (side == B_taken))
         prefs.board[x][y+1] |= T_taken;
    
     if ((prefs.board[x][y] & LRTB_taken) == LRTB_taken)
         prefs.board[x][y] |= who;
}


// (re)draws the set sides of a cell
static void DrawCell(unsigned char x, unsigned char y)
{
	unsigned char cx = OFFSET_X + (x * CELL_X);
	unsigned char cy = OFFSET_Y + (y * CELL_Y);
	RectangleType r;
	RGBColorType  old;
	static CustomPatternType fill = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
		
	r.topLeft.x = cx + 2;
	r.topLeft.y = cy + 2;
	r.extent.x = CELL_X - 3;
	r.extent.y = CELL_Y - 3;
	
    if ((prefs.board[x][y] & (unsigned char) L_taken) == L_taken)
    {
        WinDrawLine(cx, cy, cx, cy+CELL_Y);
    }
        
    if ((prefs.board[x][y] & (unsigned char) T_taken) == T_taken)
    {
        WinDrawLine(cx, cy, cx+CELL_X, cy);
    }
        
    if ((prefs.board[x][y] & (unsigned char) R_taken) == R_taken)
    {
        WinDrawLine(cx+CELL_X, cy, cx+CELL_X, cy+CELL_Y);
    }
        
    if ((prefs.board[x][y] & (unsigned char) B_taken) == B_taken)
    {
        WinDrawLine(cx, cy+CELL_Y, cx+CELL_X, cy+CELL_Y);
    }


	if ((prefs.board[x][y] & LRTB_taken) == LRTB_taken)
	{
	    char got = ((prefs.board[x][y] & player_one_got) == 
	                  player_one_got) ? 0 : 1;
		WinSetColors(ColPlayer[got], &old, 0, 0); // WinSetForeColor			
			
		WinSetPattern(&fill);
		WinFillRectangle(&r, 0);
		
		WinSetColors(&old, NULL, 0, 0);
	}

}

static Boolean checkGameOver(void)
{
    if (turns_left() == 0)
    {
        char msg[80];
        
        StrPrintF(msg,
                  "Congratulations Player %c, You Have Won!", 
                  (prefs.flags & player_one_up) == player_one_up ? 
                      '1' : '2');
        FrmCustomAlert(MsgAlert, "Game Over!", msg, NULL);
        return true;
    }
    else
        return false;
}

// determine the computer's move
static void doP0go()
{
    unsigned char    x, y;
    unsigned char    i, j;
    Boolean free[5];
    Boolean again;
    unsigned char    foundX[5], foundY[5];
    unsigned char    go_x, go_y;    
    
    const unsigned char side[4] = { L_taken, 
                                    R_taken, 
                                    T_taken, 
                                    B_taken };
    
    do 
    {
        again = false;
        
        for (i = 0; i < 5; i++)
            free[i] = false;
        
        j = 0;
    
        // Move Types
        // 1st pref. 3 sides filled - scores
        // 2nd pref. 0 sides filled - safest
        // 3rd pref. 1 side filled  - safe
        // Finally   2 sides filled - yields points
    
        // find the 1st (if any) of each Move Type
        for (x = 0; (x < sizeX()) && (j < 5); x++)
            for (y = 0; (y < sizeY()) && (j < 5); y++)
                for (i = 1; (i < 5) && (j < 5); i++)
                    if (!free[i] && (sidesFree(prefs.board[x][y]) == i))
                    {
                        free[i] = true;
                        foundX[i] = x;
                        foundY[i] = y;
                        j++;
                        break;
                    }
    
        // return the preferred move
        if (j > 0)
        {
            if (free[1])
            {
                go_x = foundX[1];
                go_y = foundY[1];
                again = true;
            }
            else if (free[4])
            {
                go_x = foundX[4];
                go_y = foundY[4];
            }
            else if (free[2])
            {
                go_x = foundX[2];
                go_y = foundY[2];
            }
            else if (free[1])
            {
                go_x = foundX[1];
                go_y = foundY[1];
            }
            else
            {
                // No move possible - should never get here
                FrmCustomAlert(MsgAlert, "Error", "Computer could not go!", NULL);
                P0InvalidTurn();
                break;
            }
            
            for (i = 0; i < 4; i++)
            {
                if ((prefs.board[go_x][go_y] & side[i]) == 0)
                {
                    setSide(go_x, go_y, side[i], player_two_got);
                    break;
                }
            }
        }
        else
        {
            // No move possible
            FrmCustomAlert(MsgAlert, "Error", "Computer could not go!", NULL);
            P0InvalidTurn();
            break;
        }
    }
    while (again);
        
}

// determines selected edge & applies move if possible
static unsigned char CheckTurn(unsigned char scrX, unsigned char scrY)
{
	// position in cell
	unsigned char cx = (scrX-OFFSET_X) % CELL_X;
	unsigned char cy = (scrY-OFFSET_Y) % CELL_Y;
	
	unsigned char side = 0;
	unsigned char x, y;
	
	// select cell
	x = (scrX-OFFSET_X) / CELL_X;
	y = (scrY-OFFSET_Y) / CELL_Y;
	
	if ((x >= sizeX()) || 
	    (y >= sizeY()) || 
	    (cx >= CELL_X) || 
	    (cy >= CELL_Y))
	{
	    return false;
	}

	// check if tab is close to an edge
	// (using 1/3 for moment)
	if ((cy < CELL_Y/3) && // Tmb
	    (cx > CELL_X/3) && (cx <= 2*CELL_X/3)) // lMr
		side |= T_taken;
	else
	if ((cy > 2*CELL_Y/3) && // tmB
	    (cx > CELL_X/3) && (cx <= 2*CELL_X/3)) // lMr
		side |= B_taken;
	else
	if ((cx < CELL_X/3) && // Lmr
	    (cy > CELL_Y/3) && (cy <= 2*CELL_Y/3)) // tMb
		side |= L_taken;
	else
	if ((cx > 2*CELL_X/3) && // lmR
	    (cy > CELL_Y/3) && (cy <= 2*CELL_Y/3)) // tMb
		side |= R_taken;
	else
	{
		return false;
    }
		
	if ((prefs.board[x][y] & side) == side) // already set
	{
		return false;
	}
		
    setSide(x, y, side, 
            (prefs.flags & player_one_up) == player_one_up ? 
                 player_one_got : player_two_got);
		
	return true;
}


// checks that a ROM version 
static Err RomVersionCompatible(UInt32 requiredVersion, UInt16 launchFlags)
{
    UInt32 romVersion;

    // See if we're on in minimum required version of the ROM or later.
    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
    if (romVersion < requiredVersion)
    {
        if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
            (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
        {
            FrmAlert (RomIncompatibleAlert);

            // Palm OS 1.0 will continuously relaunch this app unless we switch to 
            // another safe one.
            if (romVersion < ourMinVersion)
            {
                AppLaunchWithCommand(sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL);
            }
        }

        return sysErrRomIncompatible;
    }

    return errNone;
}


// checks that a display meets minimum requirement.
static Err DisplayCompatible()
{
    // need at least gray-scale (2 bit color)
    FtrGet(sysFtrCreator, sysFtrNumDisplayDepth, &g_color_bits);
    if (g_color_bits < 2)
    {
        FrmAlert (DisplayIncompatibleAlert);
        return sysErrRomIncompatible;
    }

    return errNone;
}

static unsigned short drawScore(Boolean done)
{
    char banner[80];
    unsigned short p1Score = 0;
    unsigned short p2Score = 0;
    unsigned short ret = 0;
    char pup = ((prefs.flags & one_player) == one_player) ? 1 : 2;
    unsigned char x, y;


    for (x=0; x<sizeX(); x++)
        for (y=0; y<sizeY(); y++)
            if ((prefs.board[x][y] & (unsigned char) LRTB_taken) == 
                       LRTB_taken)
            {
                if ((prefs.board[x][y] & player_one_got) == 
	                  player_one_got)
                    p1Score += 10;
                else
                    p2Score += 10;
            }


    if (done)
        if (p1Score > p2Score)
        {
        	StrPrintF(banner, 
    	      "Player 1:>%04d<  Player 2: %04d          ", 
    	      p1Score, 
    	      p2Score);
    	    ret = p1Score;
    	}
    	else
    	{
	    	StrPrintF(banner, 
	          "Player 1: %04d   Player 2:>%04d<         ", 
	          p1Score, 
	          p2Score);
	        ret = p2Score;
	    }
    else
    	StrPrintF(banner, 
    	          "Player 1: %04d   Player 2: %04d   P%0d Up", 
    	          p1Score, 
    	          p2Score,
    	          pup);
	WinDrawChars(banner, StrLen(banner), 5, 12);
	
	return ret;
}

static unsigned short drawBoard(Boolean done)
{
    unsigned char x, y, cx, cy;    
	
	for (cx = OFFSET_X; cx < SCREEN_X; cx+=CELL_X)
		for (cy = OFFSET_Y; cy < SCREEN_Y; cy+=CELL_Y)
		{
			WinDrawLine(cx-1, cy-1, cx+1, cy-1);
			WinDrawLine(cx-1, cy-1, cx-1, cy+1);
			WinDrawLine(cx, cy, cx, cy);
			WinDrawLine(cx+1, cy-1, cx+1, cy+1);
			WinDrawLine(cx-1, cy+1, cx+1, cy+1);
		}
		
     for (x=0; x<sizeX(); x++)
        for (y=0; y<sizeY(); y++)
            DrawCell(x, y);

	return drawScore(done);
            
}

//handles collecting user prefs.
static void GetPrefs(void)
{
    short       buttonHit;
    FormPtr     prefFormP = FrmInitForm(PreferencesForm);
    
    ListPtr     listP = FrmGetObjectPtr(prefFormP,
                            FrmGetObjectIndex(prefFormP, 
                                PreferencesLiColorsList));
    short       playerGrp = ((prefs.flags & one_player) == one_player) ? 
                                PreferencesPOnePushButton : 
                                PreferencesPTwoPushButton ;
    short       sizeGrp = (prefs.flags & szMask) >> 1;
    //unsigned short newSz;
    short       colorItem;
    const char  colItemMap[7] = {r_g, r_b, g_r, g_b, b_r, b_g};
    static char  ld[11] = "Light/Dark";
    static char  dl[11] = "Dark/Light";
    char  *mono_List[2] = {ld, dl};

    static char  rg[11] = "Red/Green ";
    static char  rb[11] = "Red/Blue  ";
    static char  gr[11] = "Green/Red ";
    static char  gb[11] = "Green/Blue";
    static char  br[11] = "Blue/Red  ";
    static char  bg[11] = "Blue/Green";
    char  *colorList[6] = {rg, rb, gr, gb, br, bg};
    unsigned char oldFlags = prefs.flags;
    unsigned char oldFlagsKeep = (oldFlags & monochrome) | 
                                 (oldFlags & player_one_up);
                                       
    FrmSetControlGroupSelection (prefFormP, 
                                 PreferencesFormGroupID, 
                                 playerGrp);

    switch (prefs.flags & (unsigned char) szMask)
    {
        case szScr1 : sizeGrp = PreferencesSize4PushButton; break;
        case szScr2 : sizeGrp = PreferencesSize3PushButton; break;
        case szScr3 : sizeGrp = PreferencesSize2PushButton; break;
        case szScr4 : sizeGrp = PreferencesSize1PushButton; break;
    }

    FrmSetControlGroupSelection (prefFormP, 
                                 PreferencesFormGroupID2, 
                                 sizeGrp);
    
    if ((prefs.flags & monochrome) == monochrome)        
    {
        LstSetListChoices(listP, 
                          mono_List, 
                          2);
        LstSetSelection(listP, (prefs.flags & d_l) == d_l ? 2 : 1);
    }
    else
    {
        short p;
        LstSetListChoices(listP, 
                          colorList, 
                          6);
                          
        switch (prefs.flags & color_mask)
        {
            case r_g : p = 0; break;
            case r_b : p = 1; break;
            case g_r : p = 2; break;
            case g_b : p = 3; break;
            case b_r : p = 4; break;
            case b_g : p = 5; break;
            default  : p = 0;
        }
        LstSetSelection(listP, p);
    }
    prefs.flags = oldFlagsKeep;
    
    // Put up the form
    buttonHit = FrmDoDialog(prefFormP);
        
    // Collect new setting
    if ( buttonHit == PreferencesOKButton )
    {
        if (FrmGetObjectId(prefFormP,
                FrmGetControlGroupSelection(prefFormP, 
                    PreferencesFormGroupID)) == PreferencesPOnePushButton)
        {
            prefs.flags |= one_player;
        }

        colorItem = LstGetSelection(listP);
        
        if ((prefs.flags & monochrome) == monochrome)
        {
            if (colorItem == 0)
                prefs.flags |= d_l;            
        }
        else
        {
            prefs.flags |= colItemMap[colorItem];
        }
        setColors();
        
        switch (FrmGetObjectId(prefFormP,
                FrmGetControlGroupSelection(prefFormP, 
                    PreferencesFormGroupID2)))
        {
            case PreferencesSize1PushButton : prefs.flags |= szScr4; 
                                              break;
            case PreferencesSize2PushButton : prefs.flags |= szScr3; 
                                              break;
            case PreferencesSize3PushButton : prefs.flags |= szScr2; 
                                              break;
            case PreferencesSize4PushButton : prefs.flags |= szScr1; 
                                              break;
        }
    }
    // Delete the form
    FrmDeleteForm(prefFormP);   
}

// displays the Hi-Scores
static void ShowScores(void)
{
    FormPtr formP = FrmInitForm(HiScoreForm);
    
    ListPtr listP = FrmGetObjectPtr(formP,
                            FrmGetObjectIndex(formP,
                                HiScoreLScoresList));
    char   *listItems[10] = {prefs.score[0].name,
                             prefs.score[1].name,
                             prefs.score[2].name,
                             prefs.score[3].name,
                             prefs.score[4].name,
                             prefs.score[5].name,
                             prefs.score[6].name,
                             prefs.score[7].name,
                             prefs.score[8].name,
                             prefs.score[9].name};

    LstSetListChoices(listP, listItems, 10);

    LstSetSelection(listP, -1);
    
    // Put up the form
    (void) FrmDoDialog(formP);
    
     // Delete the form
    FrmDeleteForm(formP);
}


// main menu handling
static Boolean  MainFormDoCommand(UInt16 command)
{
    Boolean handled = false;
    FormPtr frmNP, frmP = FrmGetActiveForm();
    void *  verH;
	char *  ver;
	char    verstr[20];

    switch (command)
    {
        case OptionsAboutDotz:
            MenuEraseStatus(0);  // Clear the menu status from the display.
			verH = DmGet1Resource('tver', 1000);
			ver = MemHandleLock(verH);
			StrPrintF(verstr, "Version %s", ver);			
            frmNP = FrmInitForm (AboutForm);
            FrmNewLabel(&frmNP, 1104, verstr, 50, 104, boldFont);
            FrmDoDialog (frmNP);              // Display the About Box.
            FrmDeleteForm (frmNP);
			MemHandleUnlock(verH);
			DmReleaseResource(verH);
            handled = true;
            break;

        case OptionsPreferences:
            MenuEraseStatus(0);  // Clear the menu status from the display.
            GetPrefs();
            FrmDrawForm (frmP);
            // drop thru'
        case OptionsNewGame:
            MenuEraseStatus(0);  // Clear the menu status from the display.
            MemSet(prefs.board, 0, MAX_X * MAX_Y);
            prefs.flags |= player_one_up; 
            drawBoard(false);
            handled = true;
            break;

        case HighScoreShowHighScores:
            MenuEraseStatus(0);                    // Clear the menu status from the display.
            frmNP = FrmInitForm (HiScoreForm);
            FrmDoDialog (frmNP);                    // Display the About Box.
            FrmDeleteForm (frmNP);
            handled = true;
            break;
    }
    
    return handled;
}

static void checkNewHS(unsigned short hs)
{
    char       i, j;
    char      *name;
    const char no_name[] = "ANONYMOUS";
    FormPtr    FormP;
    FieldPtr   fld;
    short      buttonHit;
    
    for (i = 0; i < 10; i++)
        if (prefs.score[i].points < hs)
        {
            for (j = 9; j > i; j--)
            {
                prefs.score[j].points = 
                    prefs.score[j-1].points;
                StrCopy(prefs.score[j].name, 
                    prefs.score[j-1].name);
            }
            prefs.score[i].points = hs; 
            
            FormP = FrmInitForm(HSUIForm);
            buttonHit = FrmDoDialog(FormP);

            fld = FrmGetObjectPtr(FormP, 
                                  FrmGetObjectIndex(FormP,
                                                    HSUIPlayerNameField));
            name = FldGetTextPtr(fld);
            if (name == NULL)
                name = (char *)no_name;
            
            StrCopy(prefs.score[i].name, name);
            FrmDeleteForm(FormP);
            break;
        }
}				        

// event handler for the main form
static Boolean  MainFormHandleEvent(EventPtr eventP)
{
    Boolean  handled = false;
    FormPtr  frmP;

    switch (eventP->eType) 
    {
        case menuEvent:
            return MainFormDoCommand(eventP->data.menu.itemID);

        case frmOpenEvent:
            frmP = FrmGetActiveForm();
            FrmDrawForm (frmP);
            drawBoard(false);
            handled = true;
            break;
            
        case penDownEvent:
			if (turns_left() && (eventP->screenY > MENU_BAR))
			{
				if (CheckTurn(eventP->screenX, eventP->screenY))
				{
				    Boolean        g_o = false;
				    unsigned short hs;
				    
				    ValidTurn();
				    if (!checkGameOver())
				    {
    				    prefs.flags ^= player_one_up;
    				    if ((prefs.flags & one_player) == one_player)
    				    {
    				        doP0go();
    				        if (!checkGameOver())
        				        prefs.flags ^= player_one_up;
        				    else
        				        g_o = true;
    				    }
				    }
				    else
				        g_o = true;
				        
				    hs = drawBoard(g_o);
				    
				    if (g_o)
				    {
				        checkNewHS(hs);
				    }
				    
				}
				else
				{
					InvalidTurn();
			    }
			}
    		handled = true;
			break;

        default:
            break;
        
    }
    
    return handled;
}

// Basically standard code from here on ....

static Boolean  AppHandleEvent(EventPtr eventP)
{
    UInt16  formId;
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
    UInt16    error;
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


static Err AppStart(void)
{
    UInt16 prefsSize;

    // Read the saved preferences / saved-state information.
    if (PrefGetAppPreferences(appFileCreator, appPrefID, &prefs, &prefsSize, true) == 
        noPreferenceFound)
    {
        UInt32 depth = g_color_bits;
        
        // 1st run, default the prefs
        MemSet(&prefs, 0, sizeof(prefs));
        
        // setup for b/w or color as earlier determined
        WinScreenMode(winScreenModeSet, 0, 0, &depth, 0);

        //FtrGet(sysFtrCreator, sysFtrNumDisplayDepth, &color_bits);
        if (g_color_bits == 2)
        {
            prefs.flags = one_player | l_d | szScr1 | monochrome | player_one_up;
        }
        else
        {
            prefs.flags = one_player | r_g | szScr1 | color      | player_one_up;
        }
            
    }
    setColors();
    
    return errNone;
}


static void AppStop(void)
{
    // Write the saved preferences / saved-state information.  This data 
    // will be backed up during a HotSync.
    PrefSetAppPreferences (appFileCreator, appPrefID, appPrefVersionNum, 
        &prefs, sizeof (prefs), true);
        
    // Close all the open forms.
    FrmCloseAllForms ();
}


// main entry point for the application.
static UInt32 StarterPalmMain(UInt16 cmd, MemPtr /*cmdPBP*/, UInt16 launchFlags)
{
    Err error;

    error = RomVersionCompatible (ourMinVersion, launchFlags);
    if (error) return (error);
    error = DisplayCompatible ();
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
    
    return errNone;
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
UInt32 PilotMain( UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    return StarterPalmMain(cmd, cmdPBP, launchFlags);
}

//