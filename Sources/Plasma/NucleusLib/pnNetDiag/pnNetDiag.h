/*==LICENSE==*

CyanWorlds.com Engine - MMOG client, server and tools
Copyright (C) 2011  Cyan Worlds, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Additional permissions under GNU GPL version 3 section 7

If you modify this Program, or any covered work, by linking or
combining it with any of RAD Game Tools Bink SDK, Autodesk 3ds Max SDK,
NVIDIA PhysX SDK, Microsoft DirectX SDK, OpenSSL library, Independent
JPEG Group JPEG library, Microsoft Windows Media SDK, or Apple QuickTime SDK
(or a modified version of those libraries),
containing parts covered by the terms of the Bink SDK EULA, 3ds Max EULA,
PhysX SDK EULA, DirectX SDK EULA, OpenSSL and SSLeay licenses, IJG
JPEG Library README, Windows Media SDK EULA, or QuickTime SDK EULA, the
licensors of this Program grant you additional
permission to convey the resulting work. Corresponding Source for a
non-source form of such a combination shall include the source code for
the parts of OpenSSL and IJG JPEG Library used as well as that of the covered
work.

You can contact Cyan Worlds, Inc. by email legal@cyan.com
 or by snail mail at:
      Cyan Worlds, Inc.
      14617 N Newport Hwy
      Mead, WA   99021

*==LICENSE==*/
/*****************************************************************************
*
*   $/Plasma20/Sources/Plasma/NucleusLib/pnNetDiag/pnNetDiag.h
*   
***/

#ifndef PLASMA20_SOURCES_PLASMA_NUCLEUSLIB_PNNETDIAG_PNNETDIAG_H
#define PLASMA20_SOURCES_PLASMA_NUCLEUSLIB_PNNETDIAG_PNNETDIAG_H

#define PNNETDIAG_INCLUDED

#ifdef PLNETGAMELIB_INCLUDED
#error "pnNetDiag and plNetGameLib libraries may not be included in the same project because they invalidate each other's pnNetCli settings"
#endif


/*****************************************************************************
*
*   Module
*
***/

void NetDiagInitialize ();
void NetDiagDestroy ();


/*****************************************************************************
*
*   NetDiag
*
***/

struct NetDiag;

NetDiag * NetDiagCreate ();
void NetDiagDelete (NetDiag * diag);
void NetDiagSetHost (
    NetDiag *       diag,
    ENetProtocol    protocol,
    const wchar_t     name[]
);

typedef void ( __cdecl * FNetDiagDumpProc)(
    const wchar_t fmt[],
    ...
);
typedef void (*FNetDiagTestCallback)(
    NetDiag *       diag,
    ENetProtocol    protocol,
    ENetError       result,
    void *          param
);


//============================================================================
// Test: SYS
//  Gather system information
//============================================================================
void NetDiagSys (
    NetDiag *               diag,
    FNetDiagDumpProc        dump,
    FNetDiagTestCallback    callback,
    void *                  param
);

//============================================================================
// Test: DNS
//  Lookup server address
//============================================================================
void NetDiagDns (
    NetDiag *               diag,
    ENetProtocol            protocol,
    FNetDiagDumpProc        dump,
    FNetDiagTestCallback    callback,
    void *                  param
);

//============================================================================
// Test: ICMP
//  Send out 5 sequential ICMP ping packets to the server
//============================================================================
void NetDiagIcmp (
    NetDiag *               diag,
    ENetProtocol            protocol,
    FNetDiagDumpProc        dump,
    FNetDiagTestCallback    callback,
    void *                  param
);

//============================================================================
// Test: TCP
//  Connect to server and measure bandwidth
//============================================================================
void NetDiagTcp (
    NetDiag *               diag,
    ENetProtocol            protocol,
    unsigned                port,   // 0 --> use default client port
    FNetDiagDumpProc        dump,
    FNetDiagTestCallback    callback,
    void *                  param
);
        

#endif // PLASMA20_SOURCES_PLASMA_NUCLEUSLIB_PNNETDIAG_PNNETDIAG_H
