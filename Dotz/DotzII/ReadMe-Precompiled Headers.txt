###############################################################################
#
# Copyright (c) 1999 Palm Computing, Inc. or its subsidiaries.
# All rights reserved.
#
# File: ReadMe-Precompiled Headers
#
###############################################################################

This file contains notes regarding how to use Precompiled Headers with the SDK.

To use precompiled headers, add PalmOS.pch to your application project,
and add the output filename ("PalmOS_Headers" for device builds, or
"PalmOS_Sim_Headers" for simulator builds) to the "Prefix File" field
of the "C/C++ Language" project settings panel.

To use a custom prefix file in your project, #include "StarterPrefix.h" and
in "StarterPrefix.h" add: #include "PalmOS_Headers" (or PalmOS_Sim_Headers)
as the FIRST #include line.

To override values (such as the ERROR_CHECK_LEVEL define), see comments in
<PalmOS.pch> regarding modifying PalmOS.pch or creating your own version.

