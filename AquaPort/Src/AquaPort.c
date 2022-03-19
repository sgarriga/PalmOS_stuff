// AquaPort : Copyright (c) 2001, Stephen Garriga.  
// All rights reserved.
 
#include <Pilot.h>
#include <SysEvtMgr.h>
#include <FeatureMgr.h>
#include <DateTime.h>
#include <Rect.h>
#include <Form.h>
//#include <Font.h>
#include <WindowNew.h>
#include <ScrDriverNew.h>
#include <UIcommon.h>
#include <SerialMgr.h>
#include <HsExt.h>
//#include <MathLib.h>
// ^^ used temporarily for ceil
#undef  serDefaultCTSTimeout
// ^^^^^ in SerMgr.h this has a trailing semicolon!
#define serDefaultCTSTimeout     (5*sysTicksPerSecond)

//typedef unsigned char	Boolean;

#define RxByteTimeout            300

#include "AquaPortRsc.h"

/***********************************************************************
 *   Global Defines/Macros
 **********************************************************************/
// Uncomment to generate test data by pressing 'D/L'
///#define DUMMY_DOWNLOAD


#define HI(x)    ((x >> 4) & 0x0f)
#define LO(x)    (x & 0x0f)

#define METRIC_FLAG       (unsigned char) 0xAD
#define IMPERIAL_FLAG     (unsigned char) 0xA6
#define MORE_DATA_FLAG    (unsigned char) 0xEF
#define NO_DATA_FLAG      (unsigned char) 0xFF

// YYYYMMDDHHmm\0 
#define  NATO_DATE_SIZE   15

// DD/MM/YYYY HH:mm:ss\0 or MM/DD/YYYY HH:mm:ss\0
#define  NORM_DATE_SIZE   20

// Limit # logs
#define  MAX_LOGS         50

// Aqualand Tx Buffer Size
#define  BLOCK_SIZE       32

// 'round' number > largest possible record size
#define  MAX_DATA         4096

// Application Identity Stuff
#define appFileCreator             'SGAP'
#define appDBVersionNum            0x01
#define appPrefID                  0x00
#define appPrefVersionNum          0x01
#define appDBName                  "AquaPortDB"
#define appDBType                  'DATA'

/***********************************************************************
 *   Internal Structures
 **********************************************************************/

typedef enum 
{ 
  US,       // MM/DD/YYYY HH:mm:ss
  UK,       // DD/MM/YYYY HH:mm:ss
  NATO      // YYYYMMDDHHmmss       *easiest to sort
} dateFormType; 

typedef enum 
{ 
  text_view=0,
  graph_view=1,
  raw_view=2,
  // add new views above & keep numbers correct
  max_view=3 // not a real view!
} viewType;
 
typedef enum 
{ 
  metric_depth, 
  metric_temp, 
  imperial_depth, 
  imperial_temp,
  duration 
} unitsType;

typedef enum
{
  plot_line,
  plot_point
} plotStyleType;

typedef struct 
{
    dateFormType  dateFormat;
    viewType      view;
    plotStyleType plotStyle;
    short         showTemp;
    short         showMins;
} PreferenceType;

typedef struct 
{
    char          key[NATO_DATE_SIZE]; //YYYYMMDDHHmmss<nul>
    short         blocks : 16;
    unsigned char data[MAX_DATA]; 
} LogRecType;

typedef LogRecType* LogRecPtr;

/***********************************************************************
 *   Global variables
 **********************************************************************/

// Prefs
static PreferenceType gPrefs;

// no. of records in DB
static UInt           gRecCnt = 0;

// single instance of record, used & recycled
static LogRecType     gRec;

// handle for set of list entries
static VoidHand       gStringArrayH = NULL;

// set of list entry strings
static char           gKeyString[NORM_DATE_SIZE * MAX_LOGS];

// which form was the 'Options' form called from
static Word           gOptionsCalledFrom;

/***********************************************************************
 *   Internal Constants
 **********************************************************************/

// area of screen used by visualisations
const RectangleType   viewArea = {{01, 14}, {158, 120}}; 

// area of screen used by graphs
const RectangleType   plotArea = {{20, 20}, {120, 100}};

// guess at number of bytes used for gRec.key & gRec.blocks
const int             logRecSize = sizeof(LogRecType);

// an empty pattern used to wipe the visualisation area
const CustomPatternType pattern = {0x0000, 0x0000, 0x0000, 0x0000};

// a dummy date used to initialise the separators in UK & US dates
const char nullDate[NORM_DATE_SIZE] = {"xx/xx/YYYY HH:mm:ss"};

// a dummy date used to initialise NATO dates
const char nullNATODate[NATO_DATE_SIZE] = {"YYYYMMDDhhmmss"};

const UInt DBAttr = dmModeReadWrite | dmModeShowSecret;

/***********************************************************************
 *   Internal Functions
 **********************************************************************/

/***********************************************************************
 *
 * FUNCTION:    CompareDates
 *
 * DESCRIPTION: Compares 2 keys
 *
 **********************************************************************/
static Int CompareDates(LogRecType *rec1, 
                        LogRecType *rec2,
                        Int unused1, 
                        SortRecordInfoPtr unused2, 
                        SortRecordInfoPtr unused3,
                        VoidHand unused4)
{
    return StrCompare(rec1->key, rec2->key);
}


/***********************************************************************
 *
 * FUNCTION:    DateFormat
 *
 * DESCRIPTION: Return a pointer to a formatted date string
 *
 * PARAMETERS:  NATO standard date string pointer
 *
 * RETURNS:     formatted date string pointer, or input
 *
 **********************************************************************/
static CharPtr DateFormat(CharPtr NATOdate)
{
    static char pDate[NORM_DATE_SIZE];
    
    switch(gPrefs.dateFormat)
    {
        case UK:
            StrCopy(pDate, nullDate);
            StrNCopy(pDate, &NATOdate[6], 2);
            StrNCopy(&pDate[3], &NATOdate[4], 2);
            StrNCopy(&pDate[6], NATOdate, 4);
            StrNCopy(&pDate[11], &NATOdate[8], 2);
            StrNCopy(&pDate[14], &NATOdate[10], 2);
            StrNCopy(&pDate[17], &NATOdate[12], 2);
            break;

        case US:
            StrCopy(pDate, nullDate);
            StrNCopy(pDate, &NATOdate[4], 2);
            StrNCopy(&pDate[3], &NATOdate[6], 2);
            StrNCopy(&pDate[6], NATOdate, 4);
            StrNCopy(&pDate[11], &NATOdate[8], 2);
            StrNCopy(&pDate[14], &NATOdate[10], 2);
            StrNCopy(&pDate[17], &NATOdate[12], 2);
            break;

        case NATO:
        default:
            return NATOdate;
    }
    
    return pDate;
}

/***********************************************************************
 *
 * FUNCTION:    DateUnformat
 *
 * DESCRIPTION: Return a pointer to a NATO date string
 *
 * PARAMETERS:  formatted standard date string pointer
 *
 * RETURNS:     NATO standard date string pointer, or input
 *
 **********************************************************************/
static CharPtr DateUnformat(CharPtr fdate)
{
    static char pDate[NATO_DATE_SIZE];
    
    switch(gPrefs.dateFormat)
    {
        case UK:
            StrCopy(pDate, nullNATODate);
            StrNCopy(pDate, &fdate[6], 4);
            StrNCopy(&pDate[4], &fdate[3], 2);
            StrNCopy(&pDate[6], fdate, 2);
            StrNCopy(&pDate[8], &fdate[11], 2);
            StrNCopy(&pDate[10], &fdate[14], 2);
            StrNCopy(&pDate[12], &fdate[17], 2);
            break;

        case US:
            StrCopy(pDate, nullNATODate);
            StrNCopy(pDate, &fdate[6], 4);
            StrNCopy(&pDate[4], fdate, 2);
            StrNCopy(&pDate[6], &fdate[3], 2);
            StrNCopy(&pDate[8], &fdate[11], 2);
            StrNCopy(&pDate[10], &fdate[14], 2);
            StrNCopy(&pDate[12], &fdate[17], 2);
            break;

        case NATO:
        default:
            return fdate;
    }
    
    return pDate;
}

/***********************************************************************
 *
 * FUNCTION:    BuildKeyString
 *
 * DESCRIPTION: Rebuild the global (string) list of keys for the list
 *
 * PARAMETERS:  none
 *
 * RETURNS:     -
 *
 **********************************************************************/
static void BuildKeyString(void)
{
    DmOpenRef    DB;
    UInt         recNo, strEnd, i;
    LogRecPtr    recP;
    CharPtr      fdateP;
    
    DB = DmOpenDatabaseByTypeCreator(appDBType, 
                                     appFileCreator, 
                                     DBAttr);
    gRecCnt = DmNumRecords(DB);  
    gKeyString[0] = 0; // null string  
    for (recNo = 0, strEnd = 0; (recNo < gRecCnt) && (recNo < MAX_LOGS); recNo++) 
    {
        Handle  recHand;
    
        recHand = DmQueryRecord(DB, recNo);
        
        recP = MemHandleLock(recHand);
        
        fdateP = DateFormat(recP->key);
        
        for (i = 0; i < StrLen(fdateP)+1; i++) // append key 
            gKeyString[strEnd+i] = fdateP[i];  // - including null
            
        strEnd += (StrLen(fdateP) + 1);
        
        MemHandleUnlock(recHand);
    }
    
    DmCloseDatabase(DB);
}

/***********************************************************************
 *
 * FUNCTION:    RomVersionCompatible
 *
 * DESCRIPTION: This routine checks that ROM version is at least 3.1
 *
 * PARAMETERS:  - launchFlags     - flags that indicate if the UI is
 *                                  initialized.
 *
 * RETURNED:    error code or zero if ROM is compatible
 *
 **********************************************************************/
static Err RomVersionCompatible(Word launchFlags)
{
    DWord romVersionCoded;
    unsigned int romVersion;
    
    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersionCoded);
    romVersion = (sysGetROMVerMajor(romVersionCoded) * 10) + 
                     sysGetROMVerMinor(romVersionCoded);
    if (romVersion < 31) 
    {
        if ((launchFlags & 
             (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
            (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) 
        {
            FrmAlert(ROMIncompatibleAlert);
            
            // Pilot 1.0 will continuously relaunch this app unless we 
            // switch to another safe one.
            if (romVersion < 20) 
            {
                AppLaunchWithCommand(sysFileCDefaultApp, 
                                     sysAppLaunchCmdNormalLaunch, 
                                     NULL);
            }
        }
        return sysErrRomIncompatible;
    }
    return 0;
}

/***********************************************************************
 *
 * FUNCTION:    AppHandleSync
 *
 * DESCRIPTION: Handle details after the database has been synchronized.
 *
 * PARAMETERS:  none
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void AppHandleSync(void)
{
    DmOpenRef DB;
    
    // resort the DB in case conduit purged records
    DB = DmOpenDatabaseByTypeCreator(appDBType, 
                                     appFileCreator, 
                                     DBAttr);
                                     
    if (DB) 
    {
        DmQuickSort(DB, (DmComparF *) CompareDates, 0);
        DmCloseDatabase(DB);
    }
}

/***********************************************************************
 * FUNCTION:    DeleteLog
 *
 * DESCRIPTION: Confirm & mark selected record deleted
 *
 * PARAMETERS:  Record Key
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void DeleteLog(CharPtr ListKey)
{
    DmOpenRef DB;
    Word      buttonHit;
    UInt      recNo;
    
    StrCopy(gRec.key, DateUnformat(ListKey));

    buttonHit = FrmCustomAlert(ConfirmDeleteAlert, ListKey, NULL, NULL);
    
    if (buttonHit != ConfirmDeleteOK) // if del not confirmed,    
        return;                       // don't delete the record
    
    DB = DmOpenDatabaseByTypeCreator(appDBType, 
                                     appFileCreator, 
                                     DBAttr);
    
    // locate the record in the DB
    recNo = DmFindSortPosition(DB, &gRec, 0, 
                                   (DmComparF *) CompareDates, 0);
    if (recNo > 0) // record found
    {
        DmRemoveRecord(DB, recNo - 1); // remove all traces 
                                       // of the record
        gRecCnt--; 
        DmQuickSort(DB, (DmComparF *) CompareDates, 0);
    }
    else
    {
        buttonHit = FrmAlert(NoLogAlert);
    }
    
    DmCloseDatabase(DB);
}

/***********************************************************************
 * FUNCTION:    putform
 *
 * DESCRIPTION: Format 'unit' impacted value as ft or m, F or C, or min
 *
 * PARAMETERS:  - String Pointer, 
 *              - values to format (3), 
 *              - units
 *
 * RETURNED:    Formatted value placed in String Pointer
 *
 **********************************************************************/
static void putform(CharPtr str, int d1, int d2, int d3, unitsType units)
{
    switch (units)
    {
        case metric_depth:
            StrPrintF(str, "%d%d.%dm", d1, d2, d3); 
            break;

        case metric_temp:
            StrPrintF(str, "%d%d.%d%cC", d1, d2, d3, 0xaa); 
            break;

        case imperial_depth:
            StrPrintF(str, "%d%d%dft", d1, d2, d3); 
            break;

        case imperial_temp:
            StrPrintF(str, "%d%d%d%cF", d1, d2, d3, 0xaa); 
            break;

        case duration:
        default:
            StrPrintF(str, "%d%d%d min", d1, d2, d3); 
            break;
    }
}

/***********************************************************************
 * FUNCTION:    drawText
 *
 * DESCRIPTION: Display header text for selected record
 *
 * PARAMETERS:  pointer to owner Form
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void drawText(FormPtr frm)
{
    char dateStr[NORM_DATE_SIZE];
    char textStr[50];

    FrmHideObject(frm, FrmGetObjectIndex(frm, ViewScrollLButton));
    FrmHideObject(frm, FrmGetObjectIndex(frm, ViewScrollRButton));
    
    WinSetPattern((unsigned short *) &pattern);
    WinFillRectangle((RectangleType *) &viewArea, 0);
    
    StrPrintF(dateStr, "%d%d%d%d%d%d%d%d",
              HI(gRec.data[5]),
              LO(gRec.data[5]),
              HI(gRec.data[6]),
              LO(gRec.data[6]),
              HI(gRec.data[7]),
              LO(gRec.data[7]),
              HI(gRec.data[8]),
              LO(gRec.data[8]));             // YYYYMMDD
    
    StrPrintF(textStr, "%d%d:%d%d:%d%d %s",
              HI(gRec.data[10]),
              LO(gRec.data[10]),
              HI(gRec.data[11]),
              LO(gRec.data[11]),
              HI(gRec.data[12]),
              LO(gRec.data[12]),
              DateFormat(dateStr));
    WinDrawChars("Dive In", 7, 10, 25);
    WinDrawChars(textStr, StrLen(textStr), 70, 25);
    
    StrPrintF(textStr, "%d%d:%d%d:%d%d",
              HI(gRec.data[13]),
              LO(gRec.data[13]),
              HI(gRec.data[14]),
              LO(gRec.data[14]),
              HI(gRec.data[15]),
              LO(gRec.data[15]));
    WinDrawChars("Dive Out", 8, 10, 35);
    WinDrawChars(textStr, StrLen(textStr), 70, 35);
    
    StrPrintF(textStr, "Dive # %d%d",
              HI(gRec.data[9]),
              LO(gRec.data[9]));
    WinDrawChars(textStr, StrLen(textStr), 10, 15);
                          
    StrPrintF(textStr, "%d%d hrs %d%d min",
              HI(gRec.data[16]),
              LO(gRec.data[16]),
              HI(gRec.data[17]),
              LO(gRec.data[17]));
    WinDrawChars("Prior Interval", 14, 10, 45);
    WinDrawChars(textStr, StrLen(textStr), 70, 45);
    
    putform(textStr, 
            HI(gRec.data[18]),
            LO(gRec.data[18]),
            HI(gRec.data[19]),
            (gRec.data[4] == IMPERIAL_FLAG) ? 
                                 imperial_depth : metric_depth);
    WinDrawChars("Max Depth", 9, 10, 65);
    WinDrawChars(textStr, StrLen(textStr), 70, 65);

    putform(textStr, 
            LO(gRec.data[19]),
            HI(gRec.data[20]),
            LO(gRec.data[20]),
            (gRec.data[4] == IMPERIAL_FLAG) ? 
                                 imperial_temp : metric_temp);
    WinDrawChars("Min Temp.", 9, 10, 75);        
    WinDrawChars(textStr, StrLen(textStr), 70, 75);

    putform(textStr, 
            HI(gRec.data[21]),
            LO(gRec.data[21]),
            HI(gRec.data[22]),
            (gRec.data[4] == IMPERIAL_FLAG) ?
                                 imperial_depth : metric_depth);
    WinDrawChars("Ave. Depth", 10, 10, 95);        
    WinDrawChars(textStr, StrLen(textStr), 70, 95);

    putform(textStr, 
            LO(gRec.data[22]),
            HI(gRec.data[23]),
            LO(gRec.data[23]),
            duration);
    WinDrawChars("Dive Time", 9, 10, 55);
    WinDrawChars(textStr, StrLen(textStr), 70, 55);
}

/***********************************************************************
 * FUNCTION:    drawDepthGraph
 *
 * DESCRIPTION: Display Depth graphs for selected record
 *
 * PARAMETERS:  - time offset (mins) into log, 
 *              - pointer to owner Form, 
 *              - dive duration
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void drawDepthGraph(int mins_offset, FormPtr frm, int dive_time)
{
    int   i, j, tmp;
    float currD1, currD2;
    char  tStr[20];
    
    int   offset = (mins_offset * 18);

    int   maxD = (HI(gRec.data[18]) * 100) + 
                 (LO(gRec.data[18]) * 10) + 
                 HI(gRec.data[19]);
    
    // depth (LHS) 'y' label and value range
    if (gRec.data[4] == IMPERIAL_FLAG)
    {
        WinDrawChars("ft", 2, 5, 21);
        StrPrintF(tStr, "%03d", maxD);
        WinDrawChars(tStr, StrLen(tStr), 1, 115);
    	WinDrawChars("000", 4, 1, 12);
    }
    else
    {
        WinDrawChars("m ", 2, 5, 21);
        StrPrintF(tStr, "%02d.%d", maxD/10, maxD%10);
        WinDrawChars(tStr, StrLen(tStr), 1, 115);
    	WinDrawChars("00.0", 4, 1, 12);
    }
    WinDrawLine(4, 40, 14, 40);
    
    // time slice 'x' label
    StrPrintF(tStr, "%d thru %d mins", mins_offset, mins_offset + 10);
    WinDrawChars(tStr, StrLen(tStr), 20, 122);
    
    // Scale marks on 'y' axis at 10ft or 5m
    for (i = 0; i <= maxD; i += (gRec.data[4] == IMPERIAL_FLAG)?10:50)
        WinDrawLine( 18, 20 + (100 * i/maxD), 19, 20 + (100 * i/maxD));

    
    // Depth Profile (as % of max depth)
    for (i = 0, j = 0; i < 120; i += 2, j += 3)
    {   
    	int last = 0;
    	 	
        if ((gRec.data[offset+32+j] == NO_DATA_FLAG) ||
            (gRec.data[offset+32+j] == MORE_DATA_FLAG))
            break;
            
        currD1 = (HI(gRec.data[offset+32+j]) * 100) +
                 (LO(gRec.data[offset+32+j]) * 10) +
                 HI(gRec.data[offset+33+j]);
    
        tmp = 20 + (int)(100 * currD1/maxD);
        
        switch(gPrefs.plotStyle)
        {
        	plot_line:
        		if (i == 0)
        			WinDrawLine(21 + i, tmp, 21 + i, tmp);
        		else
	        		WinDrawLine(21 + i - 1, last, 21 + i, tmp);
        		break;
        	plot_point:
        	default:
			    WinDrawLine(21 + i, tmp, 21 + i, tmp);
		}
	    last = tmp;
	         
        if ((gRec.data[offset+34+j] == NO_DATA_FLAG) ||
            (gRec.data[offset+34+j] == MORE_DATA_FLAG))
            break;
        
        currD2 = (LO(gRec.data[offset+33+j]) * 100) +
                 (HI(gRec.data[offset+34+j]) * 10) +
                 LO(gRec.data[offset+34+j]);
        
        tmp = 20 + (int)(100 * currD2/maxD);
        switch(gPrefs.plotStyle)
        {
        	case plot_line:
        		WinDrawLine(22 + i - 1, last, 22 + i, tmp);
        		break;
        	case plot_point:
        	default:
	    	    WinDrawLine(22 + i, tmp, 22 + i, tmp);
	    }
        last = tmp;
    }
    
}


/***********************************************************************
 * FUNCTION:    drawTempGraph
 *
 * DESCRIPTION: Display Temp graphs for selected record
 *
 * PARAMETERS:  - time offset (mins) into log, 
 *              - pointer to owner Form, 
 *              - offset into log where temps could start
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void drawTempGraph(int mins_offset, FormPtr frm, int tempStart)
{
	static int   datum[200];
	char  tStr[20];
    int   min_mark, max_mark, mark_interval;
    int   min_temp, max_temp;
    int   i, j;
    int   sample_cnt, scaled;
    float range, scale;
	       
    if (gRec.data[tempStart - 1] == NO_DATA_FLAG)
    	return; // we have no temp info, so what else can we do?
    		
    if (gRec.data[4] == IMPERIAL_FLAG)
    {
    	min_temp = 104; // set MAX
        max_temp = 23;  // set MIN
    }
    else
    {
       	min_temp = 400; // set MAX
        max_temp = -50; // set MIN
    }
    // these will be resolved later    

	// get all datapoints into an int array
	// - this gives us max/min and makes time slices easier
    for (i = 0, j = 0; i < 200; j += 3)
    {
        if (gRec.data[tempStart+j] == NO_DATA_FLAG)
            break;
        
        datum[i] = (HI(gRec.data[tempStart+j]) * 100) +
                   (LO(gRec.data[tempStart+j]) * 10) +
                   HI(gRec.data[tempStart+j+1]);
                   
        if (datum[i] > max_temp)
        	max_temp = datum[i];
        if (datum[i] < min_temp)
        	min_temp = datum[i];
        	
        i++;     
                                
        if (gRec.data[tempStart+j+2] == NO_DATA_FLAG) 
            break;
    
        datum[i] = (LO(gRec.data[tempStart+j+1]) * 100) +
                   (HI(gRec.data[tempStart+j+2]) * 10) +
                   LO(gRec.data[tempStart+j+2]);   
                    
        if (datum[i] > max_temp)
        	max_temp = datum[i];
        if (datum[i] < min_temp)
        	min_temp = datum[i];

        i++;

    }
    sample_cnt = i;
    
    if (gRec.data[4] == IMPERIAL_FLAG)
    {
    	// Imperial 5 degree intervals
		mark_interval = 5;
    }
    else
    {
    	// Metric 5 degree intervals
		mark_interval = 50;
    }
    
    /*
    StrPrintF(tStr, 
 	          "%d -> %d : %d",
	          min_temp,
    	      max_temp,
        	  tempStart);
        	  
  	FrmCustomAlert(DebugAlert, "Info.", tStr, NULL);
  	*/
  	
	min_mark = ((min_temp - 1) / mark_interval) * mark_interval;
	max_mark = ((max_temp / mark_interval) + 1) * mark_interval;

    if (gRec.data[4] == IMPERIAL_FLAG)
    {
    	StrPrintF(tStr, "%cF", 0xaa);
        WinDrawChars(tStr, 2, 145, 21);
        
        StrPrintF(tStr, "%03d", max_mark);
        WinDrawChars(tStr, 3, 145, 12);
        
        StrPrintF(tStr, "%03d", min_mark);
        WinDrawChars(tStr, 3, 145, 115);
    }
    else
    {
    	StrPrintF(tStr, "%cC", 0xaa);
        WinDrawChars(tStr, 2, 145, 21);
        
        StrPrintF(tStr, "%02d", max_mark/10);
        WinDrawChars(tStr, 2, 145, 12);
        
        StrPrintF(tStr, "%02d", min_mark/10);
        WinDrawChars(tStr, 2, 145, 115);
    }
    WinDrawGrayLine(145, 40, 155, 40);

    range = (float) max_mark - (float) min_mark;
    scale = (100.0F / range);
    
    for (i = min_mark; i <= max_mark; i += mark_interval)
    {
        j = 120 - (int) (((float) i - (float) min_mark) * scale);
        WinDrawLine( 140, j, 142, j);
    } 
    
    /*
    StrPrintF(tStr, 
 	          "%d -> %d : %d",
	          min_mark,
    	      max_mark,
        	  mark_interval);
  	FrmCustomAlert(DebugAlert, "Scale", tStr, NULL);	                	          
  	*/
    
    for (i = (mins_offset / 5), j = 0; 
         (j < sample_cnt) && (j < 10); 
         j+=5, i++)
    {
        scaled = 120 - (int) (((float) datum[i] - (float) min_mark) * scale);
                            
    	if ((scaled <= 120) && (scaled >= 20))
        	WinDrawGrayLine(21 + (j * 12), scaled, 79 + (j * 12), scaled);
        else
        {
        	StrPrintF(tStr, 
   	    	          "[%d] %d - %d",
       		          j,
       	    	      datum[i],
       	        	  scaled);
        	FrmCustomAlert(DebugAlert, "Bad Temp", tStr, NULL);	                	          
        }    
    }
}

/***********************************************************************
 * FUNCTION:    drawGraph
 *
 * DESCRIPTION: Display graphs for selected record
 *
 * PARAMETERS:  - time offset (mins) into log, 
 *              - pointer to owner Form, 
 *              - dive duration
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void drawGraph(int mins_offset, FormPtr frm, int dive_time)
{
    int   tempStart;
                        
    // set up form
    WinSetPattern((unsigned short *) pattern);
    WinFillRectangle((RectangleType *) &viewArea, 0);
    
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollLButton));
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollRButton));
    
    if (gRec.data[BLOCK_SIZE - 1] == NO_DATA_FLAG)
    {
        WinDrawChars("No Data To Graph!", 17, 10, 35);
        return;
    }
   
    // set tempStart to index 1st temp sample in data
   	if (gPrefs.showTemp)
   	{
        tempStart = BLOCK_SIZE + (dive_time * 18) - 5; // 18 == 1/2 * 12
                            // dive time is ceil(dive mins), so we start
                            // tempStart a little way back for safety
    
    	while (tempStart < (gRec.blocks * BLOCK_SIZE))
    	{
    	    if (gRec.data[tempStart] == MORE_DATA_FLAG)
    	    {
    	    	tempStart++; // move beyond marker
    	        break;
    	    }
    	    tempStart++;
    	}
    }
    
    //{20, 20 -> +120, +100  // Plot Area ( y/x origin, width/height)
    // by allowing 100 pixels height, we can easily scale by plotting
    // %age of max depth.
    
    // Plot area bounds
    WinDrawRectangleFrame(simpleFrame, (RectangleType *) &plotArea);
    
    // Grey minute divisions
    if (gPrefs.showMins)
    {
	    WinDrawGrayLine( 32, 21,  32, 119);
	    WinDrawGrayLine( 44, 21,  44, 119);
	    WinDrawGrayLine( 56, 21,  56, 119);
	    WinDrawGrayLine( 68, 21,  68, 119);
	    WinDrawGrayLine( 80, 21,  80, 119);
	    WinDrawGrayLine( 92, 21,  92, 119);
	    WinDrawGrayLine(104, 21, 104, 119);
	    WinDrawGrayLine(116, 21, 116, 119);
	    WinDrawGrayLine(128, 21, 128, 119);
    }
    
    // minute ticks
    WinDrawLine( 32, 121,  32, 119);
    WinDrawLine( 44, 121,  44, 119);
    WinDrawLine( 56, 121,  56, 119);
    WinDrawLine( 68, 121,  68, 119);
    WinDrawLine( 80, 121,  80, 119);
    WinDrawLine( 92, 121,  92, 119);
    WinDrawLine(104, 121, 104, 119);
    WinDrawLine(116, 121, 116, 119);
    WinDrawLine(128, 121, 128, 119);

	if ((tempStart < (gRec.blocks * BLOCK_SIZE)) && gPrefs.showTemp)
		drawTempGraph(mins_offset, frm, tempStart);
		
	drawDepthGraph(mins_offset, frm, dive_time);
}

/***********************************************************************
 * FUNCTION:    drawRaw
 *
 * DESCRIPTION: Display raw bytes for selected record
 *
 * PARAMETERS:  - block to draw
 *              - pointer to owner Form, 
 *              - dive duration
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void drawRaw(int block, FormPtr frm)
{
	char longHex[9];
	char shortHex[3];
	char head[28];
	int i, j, k;
	//fontID font;
                        
    // set up form
    WinSetPattern((unsigned short *) pattern);
    WinFillRectangle((RectangleType *) &viewArea, 0);
    
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollLButton));
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollRButton));
    
    StrPrintF(head, "Block # %d of %d", block+1, gRec.blocks);
    WinDrawChars(head, StrLen(head), 20, 25);
    //font = FntSetFont(????);
    k = 35;
    for (i = 0; i < BLOCK_SIZE; i+=8)
    {
    	for (j = 0; j < 8; j++)
    	{
    		StrPrintF(longHex, "%x", gRec.data[(block * BLOCK_SIZE)+i+j]);
    		StrCopy(shortHex, &longHex[StrLen(longHex)-2]);
    		WinDrawChars(shortHex, StrLen(shortHex), 25 + (j*15), k);
    	}
    	k+=10;
    }

    if (block+1 < gRec.blocks)
    {
        k+=5;
        //FntSetFont(font);
	    StrPrintF(head, "Block # %d", block+2);
	    WinDrawChars(head, StrLen(head), 20, k);
        //font = FntSetFont(????);
	    k+=10;
	    for (i = 0; i < BLOCK_SIZE; i+=8)
	    {
	    	for (j = 0; j < 8; j++)
	    	{
	    		StrPrintF(longHex, "%x", gRec.data[((block+1) * BLOCK_SIZE)+i+j]);
	    		StrCopy(shortHex, &longHex[StrLen(longHex)-2]);
		    	WinDrawChars(shortHex, StrLen(shortHex), 25 + (j*15), k);
	    	}
	    	k+=10;
	    }
    }
    //FntSetFont(font);
}

/***********************************************************************
 * FUNCTION:    AddRecord
 *
 * DESCRIPTION: adds record to database
 *
 * PARAMETERS:  none
 *
 * RETURNED:    zero, or error code
 *
 **********************************************************************/
static Err AddRecord(void)
{
    DmOpenRef DB;
    int       err;
    Boolean   dup = false;
    UShort    recNo;
    Handle    recHand;
    UInt      recIdx = 0;
    LogRecPtr recP2;
        
    DB = DmOpenDatabaseByTypeCreator(appDBType, 
                                     appFileCreator, 
                                     DBAttr);
    if (!DB)
        return -1;
        
    recNo = DmFindSortPosition(DB, &gRec, 0, 
                                   (DmComparF *) CompareDates, 0);

    if (gRecCnt > 0)
    {
        // is key of record (recNo-1) same?, i.e. pre-exists?
        recHand = DmQueryRecord(DB, recNo - 1); 
        recP2 = MemHandleLock(recHand);
        MemHandleUnlock(recHand);
        dup = (Boolean) (CompareDates(&gRec, recP2, 0, NULL, NULL, NULL) == 0);
    }
        
    if (!dup)
    {

        recHand = DmNewRecord(DB, 
                              &recIdx, 
                              ((gRec.blocks + 1) * BLOCK_SIZE)); 
                                // hacky, but gives enough room for key & blocks
                                // while keeping the size a 'round' number
        recP2 = MemHandleLock(recHand);
        
        DmWrite(recP2, 
                0, 
                &gRec, 
                ((gRec.blocks + 1) * BLOCK_SIZE));                                                           

        MemHandleUnlock(recHand);
        gRecCnt++;
        DmQuickSort(DB, (DmComparF *) CompareDates, 0);
    }
    else
        (void) FrmCustomAlert(RecordExistsAlert, 
                              DateFormat(gRec.key), 
                              NULL, 
                              NULL);
        
    err = DmCloseDatabase(DB);
        
    return err;
}

#ifdef DUMMY_DOWNLOAD
/***********************************************************************
 * FUNCTION:    GetDummyLog
 *
 * DESCRIPTION: Generates dummy data
 *
 * PARAMETERS:  none
 *
 * RETURNED:    
 *
 **********************************************************************/
static void GetDummyLog(void)
{
    ULong           now;
    DateTimeType    dt;
    DateTimePtr     dtPtr = &dt;

    gRec.key[0]  = 0;
    gRec.blocks  = 0;
    gRec.data[0] = 0;
    
    if (gRecCnt >= MAX_LOGS)
    {
        FrmAlert(FreeSpaceAlert);
        return;
    }
    
    now = TimGetSeconds(); // since 1/1/1904
    TimSecondsToDateTime(now, dtPtr);
    
    //                           0123   45678   9
    StrCopy((char *) gRec.data, "????\xad????\x02"
    //           x12   3   4   5   6   7   8   9 
                "???\x14\x20\x03\x01\x43\x18\x51"
    //              x   1   2   3   4   5   6   7   8   9   x   1 
                "\x23\x16\x40\x07\xef\xef\xef\xef\xef\xef\xef\xef");

    gRec.data[5] = (char) (dtPtr->year / 1000) << 4;
    gRec.data[5] |= ((dtPtr->year % 1000) / 100);
    gRec.data[6] = (char) ((dtPtr->year % 100) / 10) << 4;
    gRec.data[6] |= (dtPtr->year % 10);
    gRec.data[7] = (char) (dtPtr->month / 10) << 4;
    gRec.data[7] |= (dtPtr->month % 10);
    gRec.data[8] = (char) (dtPtr->day / 10) << 4;
    gRec.data[8] |= (dtPtr->day % 10);
    gRec.data[10] = (char) (dtPtr->hour / 10) << 4;
    gRec.data[10] |= (char) (dtPtr->hour % 10);
    gRec.data[11] = (dtPtr->minute / 10) << 4;
    gRec.data[11] |= (char) (dtPtr->minute % 10);
    gRec.data[12] = (dtPtr->second / 10) << 4;
    gRec.data[12] |= (char) (dtPtr->second % 10);
    gRec.blocks++; //1
    
    gRec.data[32]=0x18;
    gRec.data[33]=0x01;
    gRec.data[34]=0x81;
    gRec.data[35]=0x17;
    gRec.data[36]=0x81;
    gRec.data[37]=0x85;
    gRec.data[38]=0x18;
    gRec.data[39]=0x51;
    gRec.data[40]=0x85;
    gRec.data[41]=0x18;
    gRec.data[42]=0x51;
    gRec.data[43]=0x85;
    gRec.data[44]=0x18;
    gRec.data[45]=0x51;
    gRec.data[46]=0x85;
    gRec.data[47]=0x18;
    gRec.data[48]=0x41;
    gRec.data[49]=0x83;
    gRec.data[50]=0x18;
    gRec.data[51]=0x21;
    gRec.data[52]=0x81;
    gRec.data[53]=0x18;
    gRec.data[54]=0x01;
    gRec.data[55]=0x79;
    gRec.data[56]=0x17;
    gRec.data[57]=0x81;
    gRec.data[58]=0x77;
    gRec.data[59]=0x17;
    gRec.data[60]=0x61;
    gRec.data[61]=0x75;
    gRec.data[62]=0x17;
    gRec.data[63]=0x41;
    gRec.blocks++; //2

    gRec.data[64]=0x71;
    gRec.data[65]=0x17;
    gRec.data[66]=0x81;
    gRec.data[67]=0x75;
    gRec.data[68]=0x17;
    gRec.data[69]=0x41;
    gRec.data[70]=0x75;
    gRec.data[71]=0x17;
    gRec.data[72]=0x51;
    gRec.data[73]=0x75;
    gRec.data[74]=0x16;
    gRec.data[75]=0x81;
    gRec.data[76]=0x65;
    gRec.data[77]=0x16;
    gRec.data[78]=0x41;
    gRec.data[79]=0x63;
    gRec.data[80]=0x16;
    gRec.data[81]=0x21;
    gRec.data[82]=0x61;
    gRec.data[83]=0x16;
    gRec.data[84]=0x01;
    gRec.data[85]=0x59;
    gRec.data[86]=0x15;
    gRec.data[87]=0x51;
    gRec.data[88]=0x57;
    gRec.data[89]=0x15;
    gRec.data[90]=0x61;
    gRec.data[91]=0x55;
    gRec.data[92]=0x15;
    gRec.data[93]=0x51;
    gRec.data[94]=0x54;
    gRec.data[95]=0x15;
    gRec.blocks++; //3
    
    gRec.data[96]=0x81;
    gRec.data[97]=0x75;
    gRec.data[98]=0x17;
    gRec.data[99]=0x41;
    gRec.data[100]=0x75;
    gRec.data[101]=0x17;
    gRec.data[102]=0x51;
    gRec.data[103]=0x75;
    gRec.data[104]=0x16;
    gRec.data[105]=0x81;
    gRec.data[106]=0x65;
    gRec.data[107]=0x16;
    gRec.data[108]=0x41;
    gRec.data[109]=0x63;
    gRec.data[110]=0x16;
    gRec.data[111]=0x21;
    gRec.data[112]=0x61;
    gRec.data[113]=0x16;
    gRec.data[114]=0x01;
    gRec.data[115]=0x59;
    gRec.data[116]=0x15;
    gRec.data[117]=0x51;
    gRec.data[118]=0x57;
    gRec.data[119]=0x15;
    gRec.data[120]=0x61;
    gRec.data[121]=0x55;
    gRec.data[122]=0x15;
    gRec.data[123]=0x51;
    gRec.data[124]=0x54;
    gRec.data[125]=0x15;
    gRec.data[126]=0x71;
    gRec.data[127]=0x58;
    gRec.blocks++; //4
    
    gRec.data[128]=0x18;
    gRec.data[129]=0x01;
    gRec.data[130]=0x81;
    gRec.data[131]=0x17;
    gRec.data[132]=0x81;
    gRec.data[133]=0x85;
    gRec.data[134]=0x18;
    gRec.data[135]=0x51;
    gRec.data[136]=0x85;
    gRec.data[137]=0x18;
    gRec.data[138]=0x51;
    gRec.data[139]=0x85;
    gRec.data[140]=0x18;
    gRec.data[141]=0x51;
    gRec.data[142]=0x85;
    gRec.data[143]=0x18;
    gRec.data[144]=0x41;
    gRec.data[145]=0x83;
    gRec.data[146]=0x18;
    gRec.data[147]=0x21;
    gRec.data[148]=0x81;
    gRec.data[149]=0x18;
    gRec.data[150]=0x01;
    gRec.data[151]=0x79;
    gRec.data[152]=0x17;
    gRec.data[153]=0x81;
    gRec.data[154]=0x77;
    gRec.data[155]=0x17;
    gRec.data[156]=0x61;
    gRec.data[157]=0x75;
    gRec.data[158]=0xff;
    gRec.data[159]=0xef;
    gRec.blocks++; // 5
    
    gRec.data[160]=0x09;
    gRec.data[161]=0x50;
    gRec.data[162]=0x90;
    gRec.data[163]=0x08;
    gRec.data[164]=0x70;
    gRec.data[165]=0x80;
    gRec.data[166]=0x07;
    gRec.data[167]=0x70;
    gRec.data[168]=0x69;
    gRec.data[169]=0x05;
    gRec.data[170]=0x9f;
    gRec.data[171]=0xff;
    gRec.blocks++; // 6

    StrPrintF(gRec.key, "%d%d%d%d%d%d%d%d%d%d%d%d%d%d", 
              HI(gRec.data[5]),
              LO(gRec.data[5]),
              HI(gRec.data[6]),
              LO(gRec.data[6]),
              HI(gRec.data[7]),
              LO(gRec.data[7]),
              HI(gRec.data[8]),
              LO(gRec.data[8]),
              HI(gRec.data[10]),
              LO(gRec.data[10]),
              HI(gRec.data[11]),
              LO(gRec.data[11]),
              HI(gRec.data[12]),
              LO(gRec.data[12]));

    (void) AddRecord();

}
#endif

/***********************************************************************
 * FUNCTION:    fixLog
 *
 * DESCRIPTION: deal with '999' every 12 samples
 *
 * PARAMETERS:  none
 *
 * RETURNED:    
 *
 **********************************************************************/
static void fixLog()
{
	int j, curr, next, ave, last = 0;
	
    for (j = 0; ;j += 3)
    {   
    	 	
        if ((gRec.data[32+j] == NO_DATA_FLAG) ||
            (gRec.data[32+j] == MORE_DATA_FLAG))
            break;
            
        curr = (HI(gRec.data[32+j]) * 100) +
               (LO(gRec.data[32+j]) * 10) +
               HI(gRec.data[33+j]);
               
        // if we get a '999' average last & next value
        // - since this happens on sample 13 and then every 12,
        //   this won't happen when last = 0
        if (curr == 999)
        {
        	// just in case last sample!
        	if ((gRec.data[34+j] == NO_DATA_FLAG) ||
            	(gRec.data[34+j] == MORE_DATA_FLAG))
            	next = last;
            else
	        	next = (LO(gRec.data[33+j]) * 100) +
               		   (HI(gRec.data[34+j]) * 10) +
                	   LO(gRec.data[34+j]);
                   
            //ave = (int) (((float) (last + next) / 2.0F) + 0.5F);
            ave = (last + next) / 2;
            
            gRec.data[32+j] = (unsigned char) 
            					(((ave / 100) << 4) | ((ave % 100) / 10));
            gRec.data[33+j] = (unsigned char) 
            					(((ave % 10) << 4)  | LO(gRec.data[33+j]));
        }
    
	    last = curr;
	         
        if ((gRec.data[34+j] == NO_DATA_FLAG) ||
            (gRec.data[34+j] == MORE_DATA_FLAG))
            break;
        
        curr = (LO(gRec.data[33+j]) * 100) +
               (HI(gRec.data[34+j]) * 10) +
               LO(gRec.data[34+j]);

        if (curr == 999)
        {
        	// just in case last sample!
        	if ((gRec.data[35+j] == NO_DATA_FLAG) ||
            	(gRec.data[35+j] == MORE_DATA_FLAG))
            	next = last;
            else            
	        	next = (HI(gRec.data[35+j]) * 100) +
    	        	   (LO(gRec.data[35+j]) * 10) +
        	    	   HI(gRec.data[36+j]);
               
            //ave = (int) (((float) (last + next) / 2.0F) + 0.5F);
            ave = (last + next) / 2;
               
            gRec.data[33+j] = (unsigned char) 
            					((HI(gRec.data[33+j]) << 4) | (ave / 100));
            gRec.data[34+j] = (unsigned char) 
            					((((ave % 100) / 10) << 4) | (ave % 10));
        }
        
        last = curr;
    }
}

/***********************************************************************
 * FUNCTION:    GetLog
 *
 * DESCRIPTION: Downloads record from Aqualand Watch
 *
 * PARAMETERS:  none
 *
 * RETURNED:    
 *
 **********************************************************************/
static void GetLog(void)
{
    SerSettingsType serialSettings;
    UInt            serialPort;
    ULong           got; 
    Err             err;
    static Byte     ack[1] = { 0xFF };
    DWord           value;
    Byte            holding[BLOCK_SIZE];

#ifdef DUMMY_DOWNLOAD
    GetDummyLog();
    return;
#endif

    gRec.key[0]  = 0;
    gRec.blocks  = 0;
    gRec.data[0] = 0;
    
    if (gRecCnt >= MAX_LOGS)
    {
        FrmAlert(FreeSpaceAlert);
        return;
    }
        
    err = FtrGet (hsFtrCreator, hsFtrIDVersion, &value);
    if (!err)
        err = SysLibFind(hsLibNameBuiltInSerial, &serialPort);
    else
        err = SysLibFind("Serial Library", &serialPort);
        
    if (err)
    {
        FrmCustomAlert(SerialPortErrorAlert, 
                       "Cannot open Serial Library.", NULL, NULL);
        return;
    }
    
    err = SerOpen(serialPort, serPortDefault, 4800L); // 8N1
    if (err)
    {
        if (err == serErrAlreadyOpen)
        {
            SerClose(serialPort);
            FrmCustomAlert(SerialPortErrorAlert, 
                           "Serial port is in use by another "
                               "application, please try again later.", 
                           NULL, 
                           NULL);
        }
        else
            FrmCustomAlert(SerialPortErrorAlert, 
                           "Cannot open the serial port.", 
                           NULL, 
                           NULL);
        return;
    }
    
    serialSettings.baudRate = 4800L;
    serialSettings.flags = serSettingsFlagBitsPerChar8 |
                             serSettingsFlagStopBits1; // no parity
    serialSettings.ctsTimeout = serDefaultCTSTimeout;
       
    err = SerSetSettings(serialPort,&serialSettings); 
    if (err)
    {
        SerClose(serialPort);
        FrmCustomAlert(SerialPortErrorAlert, 
                       "Cannot assign serial settings.", 
                       NULL, 
                       NULL);
        return;
    }   
    
    SerSendFlush(serialPort); 
    SerReceiveFlush(serialPort, 0); 
    if (FrmAlert(ConfirmRxAlert) != ConfirmRxOK)
    {
    	SerClose(serialPort);
        return; // shortcut on cancel
    }
            
    SerSend(serialPort, ack, 1, &err); 
    if (!err)
        err = SerSendWait(serialPort, -1);            
    if (err)
    {
        SerClose(serialPort);
        if (err == serErrTimeOut)
                FrmCustomAlert(SerialPortErrorAlert, 
                              "Send Timeout. "
                                "Check watch is in Transmit mode.", 
                              NULL, 
                              NULL);
        else
                FrmCustomAlert(SerialPortErrorAlert, 
                              "Undefined Send Error", 
                              NULL, 
                              NULL);
        return;
    }
        
    // download data
    do
    {        
        got = SerReceive(serialPort, 
                         holding, 
                         BLOCK_SIZE, 
                         RxByteTimeout,  // 0 = no timeout
                         &err);
        MemMove(&(gRec.data[(gRec.blocks * BLOCK_SIZE)]), holding, BLOCK_SIZE);
        gRec.blocks++;
        if (err)
        {
            SerClose(serialPort);
            if (err == serErrLineErr)
            {
                    FrmCustomAlert(SerialPortErrorAlert, 
                                  "Line Error. Check connection.", 
                                  NULL, 
                                  NULL);
            }
            else if (err == serErrTimeOut)
                    FrmCustomAlert(SerialPortErrorAlert, 
                                  "Receive Timeout. "
                                    "Check watch is in Transmit mode.", 
                                  NULL, 
                                  NULL);
            else
                    FrmCustomAlert(SerialPortErrorAlert, 
                                  "Undefined Receive Error", 
                                  NULL, 
                                  NULL);
            return;
        }
        
        SerSend(serialPort, ack, 1, &err); 
        if (!err)
            err = SerSendWait(serialPort, -1);            
        if (err)
        {
            SerClose(serialPort);
            if (err == serErrTimeOut)
                    FrmCustomAlert(SerialPortErrorAlert, 
                                  "Send Timeout. "
                                    "Check watch is in Transmit mode.", 
                                  NULL, 
                                  NULL);
            else
                    FrmCustomAlert(SerialPortErrorAlert, 
                                  "Undefined Send Error", 
                                  NULL, 
                                  NULL);
            return;
        }
        
        if (got != BLOCK_SIZE)
        {
        	SerClose(serialPort);
            FrmCustomAlert(SerialPortErrorAlert, 
                           "Incomplete Data Block", 
                           NULL, 
                           NULL);                    
            return;
        }
        
        EvtResetAutoOffTimer();
        
    } 
    while (gRec.data[(gRec.blocks * BLOCK_SIZE)-1] != NO_DATA_FLAG);

    SerSend(serialPort, ack, 1, &err); 
    if (!err)
    {
        SerSendWait(serialPort, -1);
    }
    else
	    err = 0; // got the data OK so ignore error
           
    SerClose(serialPort);
    
    if (!err)
        StrPrintF(gRec.key, "%d%d%d%d%d%d%d%d%d%d%d%d%d%d", 
                  HI(gRec.data[5]),
                  LO(gRec.data[5]),
                  HI(gRec.data[6]),
                  LO(gRec.data[6]),
                  HI(gRec.data[7]),
                  LO(gRec.data[7]),
                  HI(gRec.data[8]),
                  LO(gRec.data[8]),
                  HI(gRec.data[10]),
                  LO(gRec.data[10]),
                  HI(gRec.data[11]),
                  LO(gRec.data[11]),
                  HI(gRec.data[12]),
                  LO(gRec.data[12]));

    if (!err)
    {
    	fixLog();
        err = AddRecord();
    }
}

/***********************************************************************
 * FUNCTION:    OptionsFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the "Options" 
 *              form of this application.
 *
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event was handled and should not be passed
 *              to a higher level handler.
 *
 **********************************************************************/
static Boolean OptionsFormHandleEvent(EventPtr eventP)
{
    Boolean      handled = false;
    FormPtr      frmP = FrmGetActiveForm();
    static Word  dateGrpId;
    static Word  plotGrpId;
    ControlPtr   chkTemp = 
                     FrmGetObjectPtr(frmP, 
                                     FrmGetObjectIndex(frmP, 
                                         OptionsTempCheckbox));
    ControlPtr   chkMins = 
                     FrmGetObjectPtr(frmP, 
                                     FrmGetObjectIndex(frmP, 
                                         OptionsMinsCheckbox));
        
    switch (eventP->eType) 
    {
        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) 
            {
                case OptionsOptionsButtonOKButton:
                    dateGrpId = FrmGetObjectId(frmP,
                    	FrmGetControlGroupSelection(frmP, 
                                                	OptionsFormGroupID2));
                    switch (dateGrpId)
                    {
                        case OptionsPushDateUSPushButton:
                            gPrefs.dateFormat = US;
                            break;
                            
                        case OptionsPushDateUKPushButton:
                            gPrefs.dateFormat = UK;
                            break;
                            
                        case OptionsPushDateNATOPushButton:
                            gPrefs.dateFormat = NATO;
                            break;
                    }
                    plotGrpId = FrmGetObjectId(frmP,
                    	FrmGetControlGroupSelection(frmP, 
                                                	OptionsFormGroupID3));
                    switch (plotGrpId)
                    {
                        case OptionsPushLinePushButton:
                            gPrefs.plotStyle = plot_line;
                            break;
                            
                        case OptionsPushPointPushButton:
                            gPrefs.plotStyle = plot_point;
                            break;
                    }
                    gPrefs.showTemp = CtlGetValue(chkTemp);
                    gPrefs.showMins = CtlGetValue(chkMins);
                    FrmGotoForm(gOptionsCalledFrom);
                    handled = true;
                    break;
                    
                default:
                    handled = false;
            }
            break;
            
       case frmOpenEvent:
            switch(gPrefs.dateFormat)
            {
                case UK:
                    dateGrpId = OptionsPushDateUKPushButton;
                    break;
        
                case US:
                    dateGrpId = OptionsPushDateUSPushButton;
                    break;
                    
                default:
                case NATO:
                    dateGrpId = OptionsPushDateNATOPushButton;
                    break;
            }
            FrmSetControlGroupSelection (frmP, 
                                         OptionsFormGroupID2, 
                                         dateGrpId);
            switch(gPrefs.plotStyle)
            {
                case plot_line:
                    plotGrpId = OptionsPushLinePushButton;
                    break;
        
                default:
                case plot_point:
                    plotGrpId = OptionsPushPointPushButton;
                    break;
            }
            FrmSetControlGroupSelection (frmP, 
                                         OptionsFormGroupID3, 
                                         plotGrpId);
            CtlSetValue(chkTemp, gPrefs.showTemp);
            CtlSetValue(chkMins, gPrefs.showMins);
            FrmDrawForm(frmP);
            handled = true;
            break;

       default:
            break;
    }

    return handled;
}

/***********************************************************************
 * FUNCTION:    ShowAbout
 *
 * DESCRIPTION: This procedure handles the About ... info
 *
 * PARAMETERS:  none
 *
 * RETURNED:    true
 *
 **********************************************************************/
static Boolean ShowAbout(void)
{
    Word        buttonHit;
    Handle      verH, namH;
    CharPtr     ver, nam;
       
    namH = DmGet1Resource('tAIN', 1000);
    verH = DmGet1Resource('tver', 1000);
    nam = MemHandleLock(namH);
    ver = MemHandleLock(verH);
    buttonHit = FrmCustomAlert(AboutAlert, nam, ver, NULL);
    MemHandleUnlock(namH);
    MemHandleUnlock(verH);
    DmReleaseResource(namH);
    DmReleaseResource(verH);
    
    return true;
}

/***********************************************************************
 * FUNCTION:    SetUpList
 *
 * DESCRIPTION: This routine prepares the log list for the main form.
 *
 * PARAMETERS:  Pointers to the List, Form, List Item and List Index values
 *
 * RETURNED:    
 *
 **********************************************************************/
static void SetUpList(ListPtr *listP, FormPtr *frmP, CharPtr *keyP, Word *selection)
{

    if (gStringArrayH)
        MemHandleUnlock(gStringArrayH);
    BuildKeyString();
    *listP = FrmGetObjectPtr(*frmP,
                            FrmGetObjectIndex(*frmP, 
                                MainMainListList));
                                                         
    if (gRecCnt > 0)
    {    
        gStringArrayH = 
            SysFormPointerArrayToStrings(gKeyString, 
                                         (short) gRecCnt);
        LstSetListChoices(*listP, 
                            MemHandleLock(gStringArrayH), 
                            gRecCnt);
                            
        LstSetSelection(*listP, *selection);
        *keyP = LstGetSelectionText(*listP, *selection);
    }
    else
    {        
        gStringArrayH = NULL;
        LstSetListChoices(*listP, 
                            NULL, 
                            gRecCnt);
        LstSetSelection(*listP, (UShort) -1);
        *selection = (UShort) -1;
        *keyP = NULL;
    }
}

/***********************************************************************
 * FUNCTION:    MainFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the "Main" form 
 *              of this application.
 *
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event was handled and should not be passed
 *              to a higher level handler.
 *
 **********************************************************************/
static Boolean MainFormHandleEvent(EventPtr eventP)
{
    Boolean        handled = false;
    FormPtr        frmP = FrmGetActiveForm();
    static Word    selection = (UShort) -1;
    static ListPtr listP;
    static CharPtr keyP = NULL;

    switch (eventP->eType) 
    {
        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) 
            {
                case MainMainButtonRxButton:
                    handled = true;
                    FrmShowObject(frmP, FrmGetObjectIndex(frmP, MainDownloadingBitMap));
                    GetLog();
                    FrmHideObject(frmP, FrmGetObjectIndex(frmP, MainDownloadingBitMap));
                    SetUpList(&listP, &frmP, &keyP, &selection);
                    LstDrawList(listP);
                    break;

                case MainMainButtonDelButton:
                    if (keyP)
                    {
                        DeleteLog(keyP);
                        SetUpList(&listP, &frmP, &keyP, &selection);
                        selection = (UShort) -1;
                        LstSetSelection(listP, (UShort) -1);
                        LstDrawList(listP);
                        keyP = NULL;
                    }
                    else
                    {
                        FrmAlert(NothingSelectedAlert);
                    }
                    handled = true;
                    break;

                case MainMainButtonViewButton:
                    if (keyP)
                    {
                        StrCopy(gRec.key, DateUnformat(keyP));
                        FrmGotoForm(ViewForm);
                    }
                    else
                    {
                        FrmAlert(NothingSelectedAlert);
                    }
                    handled = true;
                    break;

                default:
                    handled = false;
            }
            break;
            
        case lstSelectEvent:
            selection = LstGetSelection(listP);
            if (selection == noListSelection)
            {
                keyP = NULL;
                selection = (UShort) -1;
            }
            else
                keyP = LstGetSelectionText(listP, selection);
            handled = true;
            break;

        case menuEvent:
            switch (eventP->data.menu.itemID) 
            {
                case MainOptionsOptions:
                    gOptionsCalledFrom = MainForm;
                    FrmGotoForm(OptionsForm);
                    LstSetSelection(listP, selection);
                    LstDrawList(listP);
                    break;
  
                case MainOptionsAbout:
                    handled = ShowAbout();
                    break;
            
                default:
                    handled = false;
                    ErrNonFatalDisplay("Unexpected Event");
            }
            break;
            
       case keyDownEvent:
               if (eventP->data.keyDown.chr == pageUpChr ||
                   eventP->data.keyDown.chr == pageDownChr)
               {
                   enum directions d = 
                       (eventP->data.keyDown.chr == pageUpChr) ? 
                           up : down;
                   LstScrollList(listP, d, 1);
               }
               handled = true;
               break;

       case frmOpenEvent:  
               SetUpList(&listP, &frmP, &keyP, &selection);             
            FrmDrawForm(frmP);
            handled = true;
            break;
            
       default:
            break;
    }

    return handled;
}

/***********************************************************************
 * FUNCTION:    drawView
 *
 * DESCRIPTION: Calls appropriate view display function
 *
 * PARAMETERS:  time offset, form, duration of dive, data block
 *
 * RETURNED:    -
 *
 **********************************************************************/
static void drawView(int offset, FormPtr frmP, int diveTime, int block)
{
    switch (gPrefs.view)
    {
        case text_view:
            drawText(frmP);
            break;
            
        case raw_view:
            drawRaw(block, frmP);
            break;
    
        case graph_view:
        default:
            drawGraph(offset, frmP, diveTime);
            break;
    }
}

/***********************************************************************
 * FUNCTION:    ViewFormHandleEvent
 *
 * DESCRIPTION: This routine is the event handler for the
 *              "View" form of this application.
 *
 * PARAMETERS:  event  - a pointer to an EventType structure
 *
 * RETURNED:    true if the event was handled and should not be passed
 *              to a higher level handler.
 *
 **********************************************************************/
static Boolean ViewFormHandleEvent(EventPtr eventP)
{
    Boolean        handled = false;
    FormPtr        frmP  = FrmGetActiveForm();
    static Word    selection;
    static CharPtr keyP = NULL;
    static int     offset = 0;
    static int     block = 0;
    LogRecPtr      recP;
    Handle         recHand;
    DmOpenRef      DB;
    int            i;
    UInt           recNo;
    static int     diveTime = 0;
                   
    switch (eventP->eType) 
    {
        case ctlSelectEvent:
            switch (eventP->data.ctlSelect.controlID) 
            {
                case ViewViewButtonToggleButton :
                    gPrefs.view = (viewType) 
                                      (((int) gPrefs.view + 1) % 
                                          max_view);
                    drawView(offset, frmP, diveTime, block);
                    handled = true;
                    break;
                    
                case ViewViewButtonOKButton :
                    handled = true;
                    diveTime = 0;
                    FrmGotoForm(MainForm);
                    break;

                case ViewScrollRButton :
                	handled = true;
                	if (gPrefs.view == graph_view)
                	{
                    	if ((offset+5) < (diveTime-5))
                        	offset += 5;
	                    drawGraph(offset, frmP, diveTime);
	                }
	                else // raw view
	                {
                    	if ((block+1) < (gRec.blocks))
                        	block++;
	                    drawRaw(block, frmP);
	                }
                    break;
                    
                case ViewScrollLButton :
                	handled = true;
                	if (gPrefs.view == graph_view)
                	{
                    	if (offset > 0)
                        	offset -= 5;
                    	drawGraph(offset, frmP, diveTime);
                    }
                    else
                    {
                    	if (block > 0)
                    		block--;
                    	drawRaw(block, frmP);
                    }
                    break;

                default:
                    handled = false;
            }
            break;
            
        case menuEvent:
            switch (eventP->data.menu.itemID) 
            {
                case MainOptionsOptions:
                    gOptionsCalledFrom = ViewForm;
                    FrmGotoForm(OptionsForm);
                    handled = true;
                    break;
  
                case MainOptionsAbout:
                    handled = ShowAbout();
                    break;
            
                default:
                    handled = false;
                    ErrNonFatalDisplay("Unexpected Event");
            }
            break;
            
       case frmOpenEvent:
            handled = true;

            DB = DmOpenDatabaseByTypeCreator(
                     appDBType, 
                     appFileCreator, 
                     DBAttr);
            recNo = DmFindSortPosition(DB, 
                                       &gRec, 
                                       0, 
                                       (DmComparF *) CompareDates, 
                                       0);
            if (recNo <= 0)
            {
                DmCloseDatabase(DB);
                (void) FrmAlert(NoLogAlert);
                FrmGotoForm(MainForm);
                break;
            }
            recHand = DmQueryRecord(DB, recNo-1);
            recP = MemHandleLock(recHand);
            StrCopy(gRec.key, recP->key);
            gRec.blocks = recP->blocks;
            for (i = 0; i < (recP->blocks * BLOCK_SIZE); i++)
                gRec.data[i] = recP->data[i];
            MemHandleUnlock(recHand);
            DmCloseDatabase(DB);
            FrmDrawForm(frmP);
            diveTime = ((gRec.data[22] & 0x0f) * 100) +
                       (((gRec.data[23] >> 4) & 0x0f) * 10) +
                       (gRec.data[23] & 0x0f);
            drawView(offset, frmP, diveTime, block);
            break;

        default:
       		handled = false;
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
 * RETURNED:    true if the event was handled and should not be passed
 *              to a higher level handler.
 *
 **********************************************************************/
static Boolean AppHandleEvent(EventPtr eventP)
{
    Word formId;
    FormPtr frmP;

    if (eventP->eType == frmLoadEvent) 
    {
        // Load the form resource.
        formId = eventP->data.frmLoad.formID;
        frmP = FrmInitForm(formId);
        FrmSetActiveForm(frmP);
        
        // Set the event handler for the form.  The handler of the 
        // currently active form is called by FrmHandleEvent each time
        // is receives an event.       
        switch (formId) 
        { 
            case MainForm:
                FrmSetEventHandler(frmP, MainFormHandleEvent);
                break;
            case ViewForm:
                FrmSetEventHandler(frmP, ViewFormHandleEvent);
                break;                
            case OptionsForm:
                FrmSetEventHandler(frmP, OptionsFormHandleEvent);
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
 * RETURNED:    -
 *
 **********************************************************************/
static void AppEventLoop(void)
{
    Word error;
    EventType event;

    do 
    {
        EvtGetEvent(&event, evtWaitForever);
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
 * DESCRIPTION:  This routine opens the application's resource file and
 *               database.
 *
 * PARAMETERS:   nothing
 *
 * RETURNED:     Err value 0 if nothing went wrong
 *
 **********************************************************************/
static Err AppStart(void)
{
    DmOpenRef         DB;
    Err               error = 0;
    Word              prefsSize = sizeof(PreferenceType);
    UInt              dbCard, dbHdrAttrs, dbVersion = appDBVersionNum;
    LocalID           local;
    
    DWord      		  depth, depthSupp, height, width;
    Boolean           color;
    
    ScrDisplayMode (scrDisplayModeGet, &width, &height, &depth, &color);
    ScrDisplayMode (scrDisplayModeGetSupportedDepths, &width, &height, &depthSupp, &color);
    if ((depthSupp > depth) && (depth < 2) && (depthSupp & 2))
    {
    	depth = 2;
	    ScrDisplayMode (scrDisplayModeSet, &width, &height, &depth, &color);
	}

    // Find the application's data file.  If it doesn't exist create it.
    DB = DmOpenDatabaseByTypeCreator(appDBType, 
                                     appFileCreator, 
                                     DBAttr);
    error = DmGetLastErr();
    if (!DB) 
    {
        error = DmCreateDatabase(0, 
                                 appDBName, 
                                 appFileCreator, 
                                 appDBType, 
                                 false);
        if (error) 
            return error;
            
        DB = DmOpenDatabaseByTypeCreator(appDBType, 
                                         appFileCreator, 
                                         DBAttr);
                                         
        // mark new DB to be backed up to PC
        DmOpenDatabaseInfo(DB, &local, NULL, NULL, &dbCard, NULL);
        DmDatabaseInfo(dbCard, 
                       local, 
                       NULL, 
                       &dbHdrAttrs,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       NULL,
                       NULL);
        dbHdrAttrs |= dmHdrAttrBackup;
        DmSetDatabaseInfo(dbCard, 
                          local, 
                          NULL, 
                          &dbHdrAttrs,
                          &dbVersion,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL);
        
        gRecCnt = 0;
    }
    else
    {
        gRecCnt = DmNumRecords(DB);
    }
    DmCloseDatabase(DB);
    
    // Read the preferences / saved-state information.  There is only 
    // one version of the Applications preferences so don't worry about
    // multiple versions.
    if (PrefGetAppPreferences(appFileCreator, 
                              appPrefID, 
                              &gPrefs, 
                              &prefsSize, 
                              true) == noPreferenceFound) 
    {
        gPrefs.dateFormat = US;
        gPrefs.view = text_view;
        gPrefs.plotStyle = plot_point;
        gPrefs.showTemp = 1;
        gPrefs.showMins = 1;
    }
    
    //error = SysLibFind(MathLibName, &MathLibRef);
    //if (error)
    //    error = SysLibLoad(LibType, MathLibCreator, &MathLibRef);
    //ErrFatalDisplayIf(error, "Can't find MathLib"); // Just an example; handle it gracefully
    //error = MathLibOpen(MathLibRef, MathLibVersion);
    //ErrFatalDisplayIf(error, "Can't open MathLib");   
    
    return error;
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
 * RETURNED:    -
 *
 **********************************************************************/
static void AppStop(void)
{
    //UInt usecount;
    //Err error;
    
    // Write the saved preferences / saved-state information.  This data
    // will be backed up during a HotSync.
    PrefSetAppPreferences (appFileCreator, appPrefID, appPrefVersionNum,
        &gPrefs, sizeof (gPrefs), true);
        
    if (gStringArrayH)
    {
        MemHandleUnlock(gStringArrayH);
    }
    
    ScrDisplayMode (scrDisplayModeSetToDefaults, NULL, NULL, NULL, NULL);
    
    //error = MathLibClose(MathLibRef, &usecount);
    //ErrFatalDisplayIf(error, "Can't close MathLib");
    //if (usecount == 0)
    //    SysLibRemove(MathLibRef); 
}

/***********************************************************************
 * FUNCTION:    SGAP_PilotMain
 *
 * DESCRIPTION: This is the main entry point for the application.
 *
 * PARAMETERS:  cmd         - word value specifying the launch code. 
 *              cmdPB       - pointer to a structure that is associated
 *                            with the launch code. 
 *              launchFlags - word value providing extra information 
 *                            about the launch.
 *
 * RETURNED:    Result of launch
 *
 **********************************************************************/
static DWord SGAP_PilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
    Err error;

    error = RomVersionCompatible(launchFlags);
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
            
        case sysAppLaunchCmdSyncNotify:
            AppHandleSync();
            break;
            
        case sysAppLaunchCmdSaveData:
            FrmSaveAllForms();
            break;
            
        case sysAppLaunchCmdInitDatabase:
            //AppLaunchCmdDatabaseInit(
            //    ((SysAppLaunchCmdInitDatabaseType *) cmdPBP)->dbP);
            break;
            
        case sysAppLaunchCmdSystemReset:
            if (((SysAppLaunchCmdSystemResetType *) 
                                            cmdPBP)->createDefaultDB)
            {
                Handle resH;

                resH = DmGet1Resource(sysResTDefaultDB, 
                                      sysResIDDefaultDB);
                if (resH) 
                {
                    DmCreateDatabaseFromImage(MemHandleLock(resH));
                    MemHandleUnlock(resH);
                    DmReleaseResource(resH);
                }
            }
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
 * PARAMETERS:  cmd         - word value specifying the launch code. 
 *              cmdPB       - pointer to a structure that is associated
 *                            with the launch code. 
 *              launchFlags - word value providing extra information 
 *                            about the launch.
 *
 * RETURNED:    Result of launch
 *
 **********************************************************************/
DWord PilotMain(Word cmd, Ptr cmdPBP, Word launchFlags)
{
    return SGAP_PilotMain(cmd, cmdPBP, launchFlags);
}

