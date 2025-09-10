// Copyleft 2018 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00		23mar18	initial version
		01		29jan19	add MIDI event message
		02		20mar20	add track property change message
		03		01apr20	add generic context menu method
		04		05apr20	add track step change message
		05		07sep20	add apply preset and part messages

		global definitions and inlines

*/

#pragma once

#include "Wrapx64.h"	// ck: special types for supporting both 32-bit and 64-bit
#include "WObject.h"	// ck: ultra-minimal base class used by many of my objects
#include "ArrayEx.h"	// ck: wraps MFC dynamic arrays, adding speed and features
#include "Round.h"		// ck: round floating point to integer

// define registry section for settings
#define REG_SETTINGS _T("Settings")

// key status bits for GetKeyState and GetAsyncKeyState
#define GKS_TOGGLED			0x0001
#define GKS_DOWN			0x8000

// clamp a value to a range
#define CLAMP(x, lo, hi) (min(max((x), (lo)), (hi)))

// trap bogus default case in switch statement
#define NODEFAULTCASE	ASSERT(0)

// load string from resource via temporary object
#define LDS(x) CString((LPCTSTR)x)

// ck: define containers for some useful built-in types
typedef CArrayEx<float, float> CFloatArray;
typedef CArrayEx<double, double> CDoubleArray;
typedef CArrayEx<char, char> CCharArray;

