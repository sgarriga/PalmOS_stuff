// AquaPort : Copyright (c) 2001, Stephen Garriga.  
// All rights reserved.
 
#include <Pilot.h>
#include <SysEvtMgr.h>
#include <FeatureMgr.h>
#include <DateTime.h>
#include <Rect.h>
#include <Form.h>
#include <ExgMgr.h>
#include <WindowNew.h>
#include <ScrDriver.h>
#include <UIcommon.h>
#include <SerialMgr.h>
#include <HsExt.h>

#undef  serDefaultCTSTimeout
// ^^^^^ in SerMgr.h this has a trailing semicolon!
#define serDefaultCTSTimeout     (5*sysTicksPerSecond)

#define RxByteTimeout            300

#include "AquaPortRsc.h"
// Config.h exists as 2 versions - 1 for each project
// - it's just a different resource header...
#include "AquaPort.h"

// Uncomment to generate test data by pressing 'D/L'
//#define DUMMY_DOWNLOAD



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

static Boolean         isColor = false;

/***********************************************************************
 *   Internal Constants
 **********************************************************************/



// guess at number of bytes used for gRec.key & gRec.blocks
const int             logRecSize = sizeof(LogRecType);

// a dummy date used to initialise the separators in UK & US dates
const char nullDate[NORM_DATE_SIZE] = {"xx/xx/YYYY HH:mm:ss"};

// a dummy date used to initialise NATO dates
const char nullNATODate[NATO_DATE_SIZE] = {"YYYYMMDDhhmmss"};

const UInt DBAttr = dmModeReadWrite | dmModeShowSecret;


void initViewPtrs(LogRecPtr gRecP, PreferenceType *gPrefP, Boolean isColor);
void drawView(int offset, FormPtr frmP, int diveTime, int block);
void scrollView(int *offset, FormPtr frmP, int diveTime, int *block, dirLRType dir);




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
CharPtr DateFormat(CharPtr NATOdate)
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
CharPtr DateUnformat(CharPtr fdate)
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
 * FUNCTION:    Beam Log
 *
 * DESCRIPTION: Beam the selected log
 *
 * PARAMETERS:  none
 *
 * RETURNS:     -
 *
 **********************************************************************/
void BeamLog()
{
    static Word    selection;
    static CharPtr keyP = NULL;
    static int     offset = 0;
    static int     block = 0;
    LogRecPtr      recP;
    Handle         recHand;
    DmOpenRef      DB;
    UInt           recNo;
    static int     diveTime = 0;
    Err            err = 0;
    
    ExgSocketType socket;
    
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
        return;
    }
    recHand = DmQueryRecord(DB, recNo-1);
    recP = MemHandleLock(recHand);
    
    MemSet(&socket, sizeof(socket), 0);
    socket.description = DateFormat(gRec.key);
    socket.type = MIME_TYPE;
    socket.name = "AquaPort.log";
    socket.target = appFileCreator;
    socket.length = MemHandleSize(recHand);
    
    err = ExgPut(&socket);
    if (!err)
    {
        ExgSend(&socket, recP, MemHandleSize(recHand), &err);
    	ExgDisconnect(&socket, err);
    }
    else
    {
	    /* oops no IR */
	    FrmCustomAlert(IRErrorAlert, "Cannot open InfraRed port.", NULL, NULL);
    }
    
    MemHandleUnlock(recHand);
    DmCloseDatabase(DB);   
         
}

/***********************************************************************
 *
 * FUNCTION:    ReceiveBeam
 *
 * DESCRIPTION: Receive a beamed log
 *
 * PARAMETERS:  socket pointer
 *
 * RETURNS:     -
 *
 **********************************************************************/
void ReceiveBeam(ExgSocketPtr socket)
{
	DmOpenRef DB;
    Err       err;
    Boolean   dup = false;
    UShort    recNo;
    Handle    recHand;
    UInt16    recIdx = 0;
    LogRecPtr recP;
    ULong     read;
    UInt      dbCard, dbHdrAttrs, dbVersion = appDBVersionNum;
    LocalID   local;
    
    gRec.key[0]  = 0;
    gRec.blocks  = 0;
    gRec.data[0] = 0;
    
    err = ExgAccept(socket);
    if (!err)
    {
    	read = ExgReceive(socket, 
    	                  &gRec, 
    	                  MAX_DATA + NATO_DATE_SIZE + 2, 
    	                  &err);
	    err = ExgDisconnect(socket, err);	                  
   	} 
    if (err)
    	return;  
    socket->goToCreator = 0; 
      
    DB = DmOpenDatabaseByTypeCreator(appDBType, 
                                     appFileCreator, 
                                     DBAttr);
    if (!DB) 
    {
        err = DmCreateDatabase(0, 
                               appDBName, 
                               appFileCreator, 
                               appDBType, 
                               false);
        if (err) 
            return;
            
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
       
    if (gRecCnt >= MAX_LOGS)
    {
        FrmAlert(FreeSpaceAlert);
        return;
    }
    
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

    recNo = DmFindSortPosition(DB, &gRec, 0, 
                                   (DmComparF *) CompareDates, 0);

    if (gRecCnt > 0)
    {
        // is key of record (recNo-1) same?, i.e. pre-exists?
        recHand = DmQueryRecord(DB, recNo - 1); 
        recP = MemHandleLock(recHand);
        MemHandleUnlock(recHand);
        dup = (Boolean) (CompareDates(&gRec, recP, 0, NULL, NULL, NULL) == 0);
    }
        
    if (!dup)
    {

        recHand = DmNewRecord(DB, 
                              &recIdx, 
                              ((gRec.blocks + 1) * BLOCK_SIZE)); 
                                // hacky, but gives enough room for key & blocks
                                // while keeping the size a 'round' number
        recP = MemHandleLock(recHand);
        
        DmWrite(recP, 
                0, 
                &gRec, 
                ((gRec.blocks + 1) * BLOCK_SIZE));                                                           

        MemHandleUnlock(recHand);
        gRecCnt++;
        DmQuickSort(DB, (DmComparF *) CompareDates, 0);
                                           
    }
    else
        (void) FrmCustomAlert(RecordExistsAlert, 
                              "this date and time", 
                              NULL, 
                              NULL);
    if (DB) 
    {
        DmCloseDatabase(DB);
    }
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
       
    ExgRegisterData(appFileCreator, exgRegTypeID, 
                    MIME_TYPE);
    
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
                                         
    ControlPtr     chkAlrm = 
                     FrmGetObjectPtr(frmP, 
                                     FrmGetObjectIndex(frmP, 
                                         OptionsAlrmCheckbox));
    if (isColor)                                     
    	FrmShowObject(frmP, FrmGetObjectIndex(frmP, OptionsAlrmCheckbox));
    else   
    {
    	FrmHideObject(frmP, FrmGetObjectIndex(frmP, OptionsAlrmCheckbox));
    	CtlSetValue(chkAlrm, false);
    }                                  
        
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
                    gPrefs.flags = 0;
                    if (CtlGetValue(chkTemp))
                    	gPrefs.flags |= SHOWTEMP;

                    	
                    if (CtlGetValue(chkMins))
                    	gPrefs.flags |= SHOWMINS;
                    	
                    if (CtlGetValue(chkAlrm))
                    	gPrefs.flags |= SHOWALRM;
                    	
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
            CtlSetValue(chkTemp, gPrefs.flags & SHOWTEMP);
            CtlSetValue(chkMins, gPrefs.flags & SHOWMINS);
            CtlSetValue(chkAlrm, gPrefs.flags & SHOWALRM);
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
                    
                case MainMainButtonBeamButton:
                	if (keyP)
                    {
                        StrCopy(gRec.key, DateUnformat(keyP));
                        FrmShowObject(frmP, FrmGetObjectIndex(frmP, MainBeamingBitMap));
                    	BeamLog();
                    	FrmHideObject(frmP, FrmGetObjectIndex(frmP, MainBeamingBitMap));
                    }
                    else
                    {
                        FrmAlert(NothingSelectedAlert);
                    }
                    handled = true;
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
                	scrollView(&offset, frmP, diveTime, &block, dir_R);
                    break;
                    
                case ViewScrollLButton :
                	handled = true;
                	scrollView(&offset, frmP, diveTime, &block, dir_L);
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
    
	ScrDisplayMode(scrDisplayModeGetSupportsColor, 0, 0, 0, &isColor);
    
    initViewPtrs(&gRec, &gPrefs, isColor);
       
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
    // one version of the applications preferences so don't worry about
    // multiple versions.
    if (PrefGetAppPreferences(appFileCreator, 
                              appPrefID, 
                              &gPrefs, 
                              &prefsSize, 
                              true) == noPreferenceFound) 
    {
        gPrefs.dateFormat = US;
        gPrefs.view = graph_view;
        gPrefs.plotStyle = plot_line;
        gPrefs.flags = SHOWTEMP | SHOWMINS | SHOWALRM;
    }    
    
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
    
    // Write the saved preferences / saved-state information.  This data
    // will be backed up during a HotSync.
    PrefSetAppPreferences (appFileCreator, appPrefID, appPrefVersionNum,
        &gPrefs, sizeof (gPrefs), true);
        
    if (gStringArrayH)
    {
        MemHandleUnlock(gStringArrayH);
    }       
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
            
        case sysAppLaunchCmdExgReceiveData:
        	{
        		if (launchFlags & sysAppLaunchFlagSubCall)
        		{       			
        			// this app was active, save data
        			FrmSaveAllForms();
        			ReceiveBeam((ExgSocketPtr) cmdPBP);	
        			FrmGotoForm(ViewForm);
        		}
				else
	        		FrmAlert(RefuseBeamAlert);

        	}
        	break;
        	
       	case sysAppLaunchCmdExgAskUser:
       		{
       			ExgAskParamPtr paramP = (ExgAskParamPtr) cmdPBP;
       			if (launchFlags & sysAppLaunchFlagSubCall)
        		{ 
        			// default, Ask
        		}
        		else
        		{
        			paramP->result = exgAskCancel; // reject if not active
        		}

       		}
       		break;
        	
    	case sysAppLaunchCmdGoTo:
    		break; // shouldn't get this but ...
           
        default:
            break;
    }    
    return 0;
}

