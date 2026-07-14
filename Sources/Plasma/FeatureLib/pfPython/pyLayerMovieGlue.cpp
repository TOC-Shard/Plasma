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

#include "pyLayerMovie.h"

#include "pyGlueHelpers.h"
#include "pyKey.h"

// glue functions
PYTHON_CLASS_DEFINITION(ptLayerMovie, pyLayerMovie);

PYTHON_DEFAULT_NEW_DEFINITION(ptLayerMovie, pyLayerMovie)
PYTHON_DEFAULT_DEALLOC_DEFINITION(ptLayerMovie)

PYTHON_INIT_DEFINITION(ptLayerMovie, args, keywords)
{
    PyObject* layerKeyObj = nullptr;
    if (!PyArg_ParseTuple(args, "O", &layerKeyObj))
    {
        PyErr_SetString(PyExc_TypeError, "__init__ expects a ptKey");
        PYTHON_RETURN_INIT_ERROR;
    }
    if (!pyKey::Check(layerKeyObj))
    {
        PyErr_SetString(PyExc_TypeError, "__init__ expects a ptKey");
        PYTHON_RETURN_INIT_ERROR;
    }
    pyKey* layerKey = pyKey::ConvertFrom(layerKeyObj);
    self->fThis->SetLayerKey(*layerKey);
    PYTHON_RETURN_INIT_OK;
}

PYTHON_METHOD_DEFINITION(ptLayerMovie, setFilename, args)
{
    ST::string filename;
    if (!PyArg_ParseTuple(args, "O&", PyUnicode_STStringConverter, &filename))
    {
        PyErr_SetString(PyExc_TypeError, "setFilename expects a string");
        PYTHON_RETURN_ERROR;
    }
    self->fThis->SetFilename(filename);
    PYTHON_RETURN_NONE;
}

PYTHON_METHOD_DEFINITION(ptLayerMovie, setFalloff, args)
{
    int minDist, maxDist;
    if (!PyArg_ParseTuple(args, "ii", &minDist, &maxDist))
    {
        PyErr_SetString(PyExc_TypeError, "setFalloff expects two ints");
        PYTHON_RETURN_ERROR;
    }
    self->fThis->SetFalloff(minDist, maxDist);
    PYTHON_RETURN_NONE;
}

PYTHON_METHOD_DEFINITION(ptLayerMovie, addCallback, args)
{
    PyObject* selfKeyObj = nullptr;
    if (!PyArg_ParseTuple(args, "O", &selfKeyObj))
    {
        PyErr_SetString(PyExc_TypeError, "addCallback expects a ptKey");
        PYTHON_RETURN_ERROR;
    }
    if (!pyKey::Check(selfKeyObj))
    {
        PyErr_SetString(PyExc_TypeError, "addCallback expects a ptKey");
        PYTHON_RETURN_ERROR;
    }
    pyKey* selfKey = pyKey::ConvertFrom(selfKeyObj);
    self->fThis->AddCallback(*selfKey);
    PYTHON_RETURN_NONE;
}

PYTHON_BASIC_METHOD_DEFINITION(ptLayerMovie, play, Play)
PYTHON_BASIC_METHOD_DEFINITION(ptLayerMovie, pause, Pause)
PYTHON_BASIC_METHOD_DEFINITION(ptLayerMovie, resume, Resume)
PYTHON_BASIC_METHOD_DEFINITION(ptLayerMovie, stop, Stop)

PYTHON_START_METHODS_TABLE(ptLayerMovie)
    PYTHON_METHOD(ptLayerMovie, setFilename, "Params: filename\nSwitches to a different movie file at runtime"),
    PYTHON_METHOD(ptLayerMovie, setFalloff, "Params: minDist,maxDist\nSets the audio falloff distances (see ptAttribSound-style Sound 3D falloff)"),
    PYTHON_METHOD(ptLayerMovie, addCallback, "Params: selfKey\nNotifies selfKey once when the movie finishes playing"),
    PYTHON_BASIC_METHOD(ptLayerMovie, play, "Plays the movie from the beginning"),
    PYTHON_BASIC_METHOD(ptLayerMovie, pause, "Pauses the movie"),
    PYTHON_BASIC_METHOD(ptLayerMovie, resume, "Resumes the movie from wherever it was paused"),
    PYTHON_BASIC_METHOD(ptLayerMovie, stop, "Stops the movie and rewinds to the beginning"),
PYTHON_END_METHODS_TABLE;

// Type structure definition
PLASMA_DEFAULT_TYPE(ptLayerMovie, "Params: layerKey\nAccessor class for a movie playing on a 3D object's material layer");

// required functions for PyObject interoperability
PyObject* pyLayerMovie::New(pyKey& layerKey)
{
    ptLayerMovie* newObj = (ptLayerMovie*)ptLayerMovie_type.tp_new(&ptLayerMovie_type, nullptr, nullptr);
    newObj->fThis->SetLayerKey(layerKey);
    return (PyObject*)newObj;
}

PYTHON_CLASS_CHECK_IMPL(ptLayerMovie, pyLayerMovie)
PYTHON_CLASS_CONVERT_FROM_IMPL(ptLayerMovie, pyLayerMovie)

///////////////////////////////////////////////////////////////////////////
//
// AddPlasmaClasses - the python module definitions
//
void pyLayerMovie::AddPlasmaClasses(PyObject* m)
{
    PYTHON_CLASS_IMPORT_START(m);
    PYTHON_CLASS_IMPORT(m, ptLayerMovie);
    PYTHON_CLASS_IMPORT_END(m);
}
