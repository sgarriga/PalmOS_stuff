/***********************************************************************
 *
 * Copyright (c) 1994-1999 3Com Corporation or its subsidiaries.
 * All rights reserved.
 *
 * PROJECT:  Pilot
 * FILE:     StarterRsc.c
 * AUTHOR:   Roger Flores: May 20, 1997
 *
 * DECLARER: Starter
 *
 * DESCRIPTION:
 *	  The list of resources used by the app are stored here.
 *
 **********************************************************************/

#include <BuildRules.h>
//#include <Pilot.h>

// RESOURCE_FILE_PREFIX is now defined in Pilot:Incs:BuildRules.h based on LANGUAGE.

char *AppResourceList[] = {
	":Rsc:"RESOURCE_FILE_PREFIX"Starter.rsrc",
	""
	};
