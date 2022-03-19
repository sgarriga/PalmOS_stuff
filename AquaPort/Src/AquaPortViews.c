// AquaPort : Copyright (c) 2001, Stephen Garriga.  
// All rights reserved.

#include <Pilot.h>
#include <Rect.h>
#include "AquaPortRsc.h"
#include "AquaPort.h"

// area of screen used by visualisations
const RectangleType   viewArea = {{01, 14}, {158, 120}}; 

// area of screen used by graphs
const RectangleType   plotArea = {{20, 20}, {120, 100}};

// an empty pattern used to wipe the visualisation area
const CustomPatternType pattern = {0x0000, 0x0000, 0x0000, 0x0000};

static LogRecPtr gRec = NULL;
static PreferenceType *gPrefs = NULL;

static const RGBColorType red = {0x00, 0xFF, 0x00, 0x00};
static const RGBColorType blue = {0x00, 0x00, 0x00, 0xFF};

static Boolean color = false;


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
              HI(gRec->data[5]),
              LO(gRec->data[5]),
              HI(gRec->data[6]),
              LO(gRec->data[6]),
              HI(gRec->data[7]),
              LO(gRec->data[7]),
              HI(gRec->data[8]),
              LO(gRec->data[8]));             // YYYYMMDD
    
    StrPrintF(textStr, "%d%d:%d%d:%d%d %s",
              HI(gRec->data[10]),
              LO(gRec->data[10]),
              HI(gRec->data[11]),
              LO(gRec->data[11]),
              HI(gRec->data[12]),
              LO(gRec->data[12]),
              DateFormat(dateStr));
    WinDrawChars("Dive In", 7, 10, 25);
    WinDrawChars(textStr, StrLen(textStr), 70, 25);
    
    StrPrintF(textStr, "%d%d:%d%d:%d%d",
              HI(gRec->data[13]),
              LO(gRec->data[13]),
              HI(gRec->data[14]),
              LO(gRec->data[14]),
              HI(gRec->data[15]),
              LO(gRec->data[15]));
    WinDrawChars("Dive Out", 8, 10, 35);
    WinDrawChars(textStr, StrLen(textStr), 70, 35);
    
    StrPrintF(textStr, "Dive # %d%d",
              HI(gRec->data[9]),
              LO(gRec->data[9]));
    WinDrawChars(textStr, StrLen(textStr), 10, 15);
                          
    StrPrintF(textStr, "%d%d hrs %d%d min",
              HI(gRec->data[16]),
              LO(gRec->data[16]),
              HI(gRec->data[17]),
              LO(gRec->data[17]));
    WinDrawChars("Prior Interval", 14, 10, 45);
    WinDrawChars(textStr, StrLen(textStr), 70, 45);
    
    putform(textStr, 
            HI(gRec->data[18]),
            LO(gRec->data[18]),
            HI(gRec->data[19]),
            (gRec->data[4] == IMPERIAL_FLAG) ? 
                                 imperial_depth : metric_depth);
    WinDrawChars("Max Depth", 9, 10, 65);
    WinDrawChars(textStr, StrLen(textStr), 70, 65);

    putform(textStr, 
            LO(gRec->data[19]),
            HI(gRec->data[20]),
            LO(gRec->data[20]),
            (gRec->data[4] == IMPERIAL_FLAG) ? 
                                 imperial_temp : metric_temp);
    WinDrawChars("Min Temp.", 9, 10, 75);        
    WinDrawChars(textStr, StrLen(textStr), 70, 75);

    putform(textStr, 
            HI(gRec->data[21]),
            LO(gRec->data[21]),
            HI(gRec->data[22]),
            (gRec->data[4] == IMPERIAL_FLAG) ?
                                 imperial_depth : metric_depth);
    WinDrawChars("Ave. Depth", 10, 10, 95);        
    WinDrawChars(textStr, StrLen(textStr), 70, 95);

    putform(textStr, 
            LO(gRec->data[22]),
            HI(gRec->data[23]),
            LO(gRec->data[23]),
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
    int    i, j, tmp, excess, last = 0;
    int    currD1, currD2;
    char     tStr[20];
    RGBColorType oldColor;
    
    int   offset = (mins_offset * 18);

    int   maxD = (HI(gRec->data[18]) * 100) + 
                 (LO(gRec->data[18]) * 10) + 
                 HI(gRec->data[19]);
    
    // depth (LHS) 'y' label and value range
    if (gRec->data[4] == IMPERIAL_FLAG)
    {
        WinDrawChars("ft", 2, 5, 21);
        StrPrintF(tStr, "%03d", maxD);
        WinDrawChars(tStr, StrLen(tStr), 1, 115);
    	WinDrawChars("000", 4, 1, 12);
    	excess = (100 * 5)/maxD; // 5 ft per 5 sec
    }
    else
    {
        WinDrawChars("m ", 2, 5, 21);
        StrPrintF(tStr, "%02d.%d", maxD/10, maxD%10);
        WinDrawChars(tStr, StrLen(tStr), 1, 115);
    	WinDrawChars("00.0", 4, 1, 12);
    	excess = (100 * 15)/maxD; // 1.5 m per 5 sec
    }
    WinDrawLine(4, 40, 14, 40);
    
    // time slice 'x' label
    StrPrintF(tStr, "%d thru %d mins", mins_offset, mins_offset + 10);
    WinDrawChars(tStr, StrLen(tStr), 20, 122);
    
    // Scale marks on 'y' axis at 10ft or 5m
    for (i = 0; i <= maxD; i += (gRec->data[4] == IMPERIAL_FLAG)?10:50)
        WinDrawLine( 18, 20 + ((100 * i)/maxD), 19, 20 + ((100 * i)/maxD));
    
#ifdef COLUR
    // set old color at safe spot
    WinSetColors(&red, &oldColor, 0, 0);
    WinSetColors(&oldColor, 0, 0, 0);
#endif
    
    // Depth Profile (as % of max depth)
    for (i = 0, j = 0; i < 120; i += 2, j += 3)
    {      	 	
        if ((gRec->data[offset+32+j] == NO_DATA_FLAG) ||
            (gRec->data[offset+32+j] == MORE_DATA_FLAG))
            break;
            
        currD1 = (HI(gRec->data[offset+32+j]) * 100) +
                 (LO(gRec->data[offset+32+j]) * 10) +
                 HI(gRec->data[offset+33+j]);
    
        tmp = 20 + ((100 * currD1)/maxD);
               
        if ((last - tmp > excess) && (gPrefs->flags & SHOWALRM))
        {
			if (color)
        		WinSetColors(&red, 0, 0, 0);
        	WinDrawLine(21 + i - 1, tmp - 5, 21 + i + 1, tmp - 5);
        	WinDrawLine(21 + i - 1, tmp - 5, 21 + i, tmp - 4);
        	WinDrawLine(21 + i, tmp - 4, 21 + i + 1, tmp - 5);
        }
        
        switch(gPrefs->plotStyle)
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
		if (color)
	    	WinSetColors(&oldColor, 0, 0, 0);
  
	         
        if ((gRec->data[offset+34+j] == NO_DATA_FLAG) ||
            (gRec->data[offset+34+j] == MORE_DATA_FLAG))
            break;
        
        currD2 = (LO(gRec->data[offset+33+j]) * 100) +
                 (HI(gRec->data[offset+34+j]) * 10) +
                 LO(gRec->data[offset+34+j]);
        
        tmp = 20 + ((100 * currD2)/maxD);

        if ((last - tmp > excess) && (gPrefs->flags & SHOWALRM))
        {
			if (color)
        		WinSetColors(&red, 0, 0, 0);

        	WinDrawLine(22 + i - 1, tmp - 5, 22 + i + 1, tmp - 5);
        	WinDrawLine(22 + i - 1, tmp - 5, 22 + i, tmp - 4);
        	WinDrawLine(22 + i, tmp - 4, 22 + i + 1, tmp - 5);
        }

        switch(gPrefs->plotStyle)
        {
        	case plot_line:
        		WinDrawLine(22 + i - 1, last, 22 + i, tmp);
        		break;
        	case plot_point:
        	default:
	    	    WinDrawLine(22 + i, tmp, 22 + i, tmp);
	    }
        last = tmp;
		if (color)
        	WinSetColors(&oldColor, 0, 0, 0);   
    }
    if (color)
    	WinSetColors(&oldColor, 0, 0, 0);
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
	char     tStr[20];
    int    min_mark, max_mark, mark_interval;
    int    min_temp, max_temp, sample_cnt, scaled;
    int    i, j;
    int    range, scale;
    RGBColorType oldColor;
    	       
    if (gRec->data[tempStart - 1] == NO_DATA_FLAG)
    	return; // we have no temp info, so what else can we do?
    		
    if (gRec->data[4] == IMPERIAL_FLAG)
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
        if (gRec->data[tempStart+j] == NO_DATA_FLAG)
            break;
        
        datum[i] = (HI(gRec->data[tempStart+j]) * 100) +
                   (LO(gRec->data[tempStart+j]) * 10) +
                   HI(gRec->data[tempStart+j+1]);
                   
        if (datum[i] > max_temp)
        	max_temp = datum[i];
        if (datum[i] < min_temp)
        	min_temp = datum[i];
        	
        i++;     
                                
        if (gRec->data[tempStart+j+2] == NO_DATA_FLAG) 
            break;
    
        datum[i] = (LO(gRec->data[tempStart+j+1]) * 100) +
                   (HI(gRec->data[tempStart+j+2]) * 10) +
                   LO(gRec->data[tempStart+j+2]);   
                    
        if (datum[i] > max_temp)
        	max_temp = datum[i];
        if (datum[i] < min_temp)
        	min_temp = datum[i];

        i++;
    }
    sample_cnt = i;
    
    if (gRec->data[4] == IMPERIAL_FLAG)
    {
    	// Imperial 5 degree intervals
		mark_interval = 5;
    }
    else
    {
    	// Metric 5 degree intervals
		mark_interval = 50;
    }
     	
	min_mark = ((min_temp - 1) / mark_interval) * mark_interval;
	max_mark = ((max_temp / mark_interval) + 1) * mark_interval;

    if (gRec->data[4] == IMPERIAL_FLAG)
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
    
	if (color)
    	WinDrawGrayLine(145, 40, 155, 40);

    range = max_mark - min_mark;
    scale = (100 / range);
    
    for (i = min_mark; i <= max_mark; i += mark_interval)
    {
        j = 120 - ((i - min_mark) * scale);
        WinDrawLine( 140, j, 142, j);
    } 

	if (color)
	{
		WinSetColors(&blue, &oldColor, 0, 0);
    	WinDrawLine(145, 40, 155, 40);
   	}
       
    for (i = (mins_offset / 5), j = 0; 
         (j < sample_cnt) && (j < 10); 
         j+=5, i++)
    {
        scaled = 120 - ((datum[i] - min_mark) * scale);
                            
    	if ((scaled <= 120) && (scaled >= 20))
			if (color)
        		WinDrawLine(21 + (j * 12), scaled, 79 + (j * 12), scaled);
			else
				WinDrawGrayLine(21 + (j * 12), scaled, 79 + (j * 12), scaled);

#ifdef DEBUG_TEMP
        else
        {
        	StrPrintF(tStr, 
   	    	          "[%d] %d - %d",
       		          j,
       	    	      datum[i],
       	        	  scaled);
        	FrmCustomAlert(DebugAlert, "Bad Temp", tStr, NULL);	                	          
        }    
#endif
	}
	if (color)	
	    WinSetColors(&oldColor, 0, 0, 0);
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
    WinSetPattern((unsigned short *) &pattern);
    WinFillRectangle((RectangleType *) &viewArea, 0);
    
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollLButton));
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollRButton));
    
    if (gRec->data[BLOCK_SIZE - 1] == NO_DATA_FLAG)
    {
        WinDrawChars("No Data To Graph!", 17, 10, 35);
        return;
    }
   
    // set tempStart to index 1st temp sample in data
   	if (gPrefs->flags & SHOWTEMP)
   	{
        tempStart = BLOCK_SIZE + (dive_time * 18) - 5; // 18 == 1/2 * 12
                            // dive time is ceil(dive mins), so we start
                            // tempStart a little way back for safety
    
    	while (tempStart < (gRec->blocks * BLOCK_SIZE))
    	{
    	    if (gRec->data[tempStart] == MORE_DATA_FLAG)
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
    if (gPrefs->flags & SHOWMINS)
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

	if ((tempStart < (gRec->blocks * BLOCK_SIZE)) && 
	    (gPrefs->flags & SHOWTEMP))
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
                        
    // set up form
    WinSetPattern((unsigned short *) &pattern);
    WinFillRectangle((RectangleType *) &viewArea, 0);
    
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollLButton));
    FrmShowObject(frm, FrmGetObjectIndex(frm, ViewScrollRButton));
    
    StrPrintF(head, "Block # %d of %d", block+1, gRec->blocks);
    WinDrawChars(head, StrLen(head), 20, 25);
    k = 35;
    for (i = 0; i < BLOCK_SIZE; i+=8)
    {
    	for (j = 0; j < 8; j++)
    	{
    		StrPrintF(longHex, "%x", gRec->data[(block * BLOCK_SIZE)+i+j]);
    		StrCopy(shortHex, &longHex[StrLen(longHex)-2]);
    		WinDrawChars(shortHex, StrLen(shortHex), 25 + (j*15), k);
    	}
    	k+=10;
    }

    if (block+1 < gRec->blocks)
    {
        k+=5;
	    StrPrintF(head, "Block # %d", block+2);
	    WinDrawChars(head, StrLen(head), 20, k);
	    k+=10;
	    for (i = 0; i < BLOCK_SIZE; i+=8)
	    {
	    	for (j = 0; j < 8; j++)
	    	{
	    		StrPrintF(longHex, "%x", gRec->data[((block+1) * BLOCK_SIZE)+i+j]);
	    		StrCopy(shortHex, &longHex[StrLen(longHex)-2]);
		    	WinDrawChars(shortHex, StrLen(shortHex), 25 + (j*15), k);
	    	}
	    	k+=10;
	    }
    }
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
void drawView(int offset, FormPtr frmP, int diveTime, int block)
{
    switch (gPrefs->view)
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
 * FUNCTION:    scrollView
 *
 * DESCRIPTION: Calls appropriate view display function
 *
 * PARAMETERS:  time offset, form, duration of dive, data block, direction
 *
 * RETURNED:    -
 *
 **********************************************************************/
void scrollView(int *offset, FormPtr frmP, int diveTime, int *block, dirLRType dir)
{
    switch (gPrefs->view)
    {
        case text_view:
            drawText(frmP);
            break;
            
        case raw_view:
        	if (dir == dir_R)
        	{
	        	if ((*block+1) < (gRec->blocks))
    	        	(*block)++;
    	    }    	    
    	    else if (dir == dir_L)
    	    {
    	    	if (*block > 0)
                    (*block)--;
    	    }
            drawRaw(*block, frmP);
            break;
    
        case graph_view:
        default:
        	if (dir == dir_R)
        	{
		        if (((*offset)+5) < (diveTime-5))
    	        	(*offset) += 5;
    	    }
    	    else if (dir == dir_L)
    	    {
    	    	if (*offset > 0)
                	(*offset) -= 5;
    	    }
            drawGraph(*offset, frmP, diveTime);
            break;
    }
}

/***********************************************************************
 * FUNCTION:    initViewPtrs
 *
 * DESCRIPTION: Initialise pointers to global data
 *
 * PARAMETERS:  - pointer to global LogRecType
 *              - pointer to global prefernces
 *              - color flag
 *
 * RETURNED:    -
 *
 **********************************************************************/
void initViewPtrs(LogRecPtr gRecP, PreferenceType *gPrefP, Boolean isColor)
{    
    gRec = gRecP;
    gPrefs = gPrefP;
    
    color = isColor;
}

    

