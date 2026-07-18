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
#ifndef _pyLayerMovie_h_
#define _pyLayerMovie_h_

//////////////////////////////////////////////////////////////////////
//
// pyLayerMovie - controls a plLayerMovie/plLayerWebM already living on a
//                3D object's material (as opposed to pyMoviePlayer, which
//                is for the full-screen movie overlay).
//
//////////////////////////////////////////////////////////////////////

#include <string_theory/string>

#include "pnKeyedObject/plKey.h"

#include "pyGlueDefinitions.h"

class pyKey;

class pyLayerMovie
{
protected:
    plKey fLayerKey;

    pyLayerMovie() = default; // only used by python glue, do NOT call
    pyLayerMovie(pyKey& layerKey);

public:
    // required functions for PyObject interoperability
    PYTHON_CLASS_NEW_FRIEND(ptLayerMovie);
    static PyObject* New(pyKey& layerKey);
    // Used internally by plPythonFileMod when delivering a ptAttribWebM parameter --
    // not part of the scripting-facing API (that's __init__, which takes a ptKey).
    static PyObject* New(plKey layerKey);
    PYTHON_CLASS_CHECK_DEFINITION; // returns true if the PyObject is a pyLayerMovie object
    PYTHON_CLASS_CONVERT_FROM_DEFINITION(pyLayerMovie); // converts a PyObject to a pyLayerMovie (throws error if not correct type)

    static void AddPlasmaClasses(PyObject* m);

    void SetLayerKey(pyKey& layerKey); // only used by python glue, do NOT call

    // Switches the layer to a different movie file at runtime.
    void SetFilename(const ST::string& filename);

    // Distances at which the movie's (3D-positioned) audio starts/finishes
    // attenuating -- see plSound::SetMin()/SetMax().
    void SetFalloff(int minDist, int maxDist);

    // Jumps to the given number of seconds from the start of the movie and keeps
    // playing from there -- doesn't change whether it's currently playing/paused.
    void SeekTo(float seconds);

    // Seconds from the start of the movie the layer is currently at. Correct even
    // if the object is currently offscreen (see plLayerMovie::MsgReceive).
    float GetPlaybackTime();

    void Play();
    void Pause();
    void Resume();
    void Stop();
};

#endif // _pyLayerMovie_h_
