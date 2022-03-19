// AquaPort : Copyright (c) 2001, Stephen Garriga.  
// All rights reserved.

#ifndef TRUE
#define TRUE (0x00==0x00)
#endif
#ifndef FALSE
#define FALSE (0x00==0xff)
#endif

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
#define appPrefVersionNum          0x02
#define appDBName                  "AquaPortDB"
#define appDBType                  'DATA'

#define MIME_TYPE                  "application/octet-stream"

/***********************************************************************
 *   Internal Structures
 **********************************************************************/

typedef enum
{
	dir_L,
	dir_R
} dirLRType;

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

// option flags
#define SHOWTEMP	1
#define SHOWMINS	2
#define SHOWALRM	4
#define TBD4		8
#define TBD5		16
#define TBD6		32
#define TBD7		64
#define TBD8		128
typedef struct 
{
    dateFormType  dateFormat;
    viewType      view;
    plotStyleType plotStyle;
    unsigned char flags;
} PreferenceType;

typedef struct 
{
    char          key[NATO_DATE_SIZE]; //YYYYMMDDHHmmss<nul>
    short         blocks : 16;
    unsigned char data[MAX_DATA]; 
} LogRecType;

typedef LogRecType* LogRecPtr;

// functions in main .c file visible to all
CharPtr DateFormat(CharPtr NATOdate);
CharPtr DateUnformat(CharPtr fdate);
