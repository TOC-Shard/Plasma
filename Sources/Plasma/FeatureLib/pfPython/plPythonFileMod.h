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
#ifndef _plPythonFileMod_h_
#define _plPythonFileMod_h_

//////////////////////////////////////////////////////////////////////
//
// plPythonFileMod   - the 'special' Python File modifier
//
// This modifier will handle the interface to python code that has been file-ized.
//
//////////////////////////////////////////////////////////////////////

#include "pnModifier/plMultiModifier.h"
#include "plPythonParameter.h"

class PythonVaultCallback;
class plPythonSDLModifier;
class pyKey;
class pfPythonKeyCatcher;
class plKeyEventMsg;
class plPipeline;

typedef struct _object PyObject;

class plPythonFileMod : public plMultiModifier
{
private:
    friend class PythonVaultCallback;

    enum func_num
    {
        kfunc_FirstUpdate = 0,
        kfunc_Update,
        kfunc_Notify,
        kfunc_AtTimer,
        kfunc_OnKeyEvent,
        kfunc_Load,
        kfunc_Save,
        kfunc_GUINotify,
        kfunc_PageLoad,
        kfunc_ClothingUpdate,
        kfunc_KIMsg,
        kfunc_MemberUpdate,
        kfunc_RemoteAvatarInfo,
        kfunc_RTChat,
        kfunc_VaultEvent,
        kfunc_AvatarPage,
        kfunc_SDLNotify,
        kfunc_OwnershipNotify,
        kfunc_AgeVaultEvent,
        kfunc_Init,
        kfunc_OnCCRMsg,
        kfunc_OnServerInitComplete,
        kfunc_OnVaultNotify,
        kfunc_OnDefaultKeyCaught,
        kfunc_OnMarkerMsg,
        kfunc_OnBackdoorMsg,
        kfunc_OnBehaviorNotify,
        kfunc_OnLOSNotify,
        kfunc_OnBeginAgeLoad,
        kfunc_OnMovieEvent,
        kfunc_OnScreenCaptureDone,
        kfunc_OnClimbBlockerEvent,
        kfunc_OnAvatarSpawn,
        kfunc_OnAccountUpdate,
        kfunc_gotPublicAgeList,
        kfunc_OnAIMsg,
        kfunc_OnGameScoreMsg,
        kfunc_lastone
    };

    template<typename T>
    T* IScriptWantsMsg(func_num methodId, plMessage* msg) const;

    /**
     * \brief Calls a bound method in this Python script.
     * \detail Calls the bound method in this Python script specified by methodId with the arguments
     *         provided as variadic template arugments.
     * \note Calling a method that is not a member of the script instance is a no-op.
     * \remarks Instance methods are resolved at initialization; therefore, Python metaprogramming
     *          hacks such as `OnNotify = new_callable` will not actually override the OnNotify
     *          method called when the plPythonFileMod receives a plNotifyMsg.
     */
    template<typename... Args>
    void ICallScriptMethod(func_num methodId, Args&&... args);

    /**
     * \brief Calls a bound method in this Python script.
     * \detail Calls the bound method in this Python script specified by methodId. This overload
     *         is used when there are no arguments to pass to the method.
     * \note Calling a method that is not a member of the script instance is a no-op.
     * \remarks Instance methods are resolved at initialization; therefore, Python metaprogramming
     *          hacks such as `OnNotify = new_callable` will not actually override the OnNotify
     *          method called when the plPythonFileMod receives a plNotifyMsg.
     */
    void ICallScriptMethod(func_num methodId);

protected:
    friend class plPythonSDLModifier;

    plPythonSDLModifier* fSDLMod;

    bool IEval(double secs, float del, uint32_t dirty);

    ST::string IMakeModuleName(plSceneObject* sobj);

    ST::string  fPythonFile;
    ST::string  fModuleName;

    // the list of receivers that want to be notified
    hsTArray<plKey>         fReceivers;

    PyObject*   fSelfKey;
    plPipeline  *fPipe;

    // the list of parameters (attributes)
    hsTArray<plPythonParameter> fParameters;

    // internal data
    PyObject*   fModule;        // python module object
    PyObject*   fInstance;      // python object that the instance of the class to run
    static bool     fAtConvertTime; // flag for when in convert time within Max, don't run code
    bool        fLocalNotify;   // True when This Mod was Notified by a local plNotify
    bool        fIsFirstTimeEval;   // flag to determine when the first time at the eval,
                                // so the Python coders can hava a chance to run initialization
                                // code after the system is up, but before things are displayed
    bool        fAmIAttachedToClone;    // is this python file mod attached to a cloned object
    
    // callback class for the KI
    PythonVaultCallback *fVaultCallback;
    pfPythonKeyCatcher  *fKeyCatcher;

    struct NamedComponent
    {
        ST::string  name;
        int32_t     id;
        bool        isActivator;
    };

    hsTArray<NamedComponent> fNamedCompQueue;

    virtual void IFindResponderAndAdd(const ST::string &responderName, int32_t id);
    virtual void IFindActivatorAndAdd(const ST::string &activatorName, int32_t id);
    void ISetKeyValue(const plKey& key, int32_t id);

    bool ILoadPythonCode();

    enum genref_whats
    {
        kNotSure = 0,
        kAddNotify
    };

public:

    plPythonFileMod();
    ~plPythonFileMod();

    CLASSNAME_REGISTER( plPythonFileMod );
    GETINTERFACE_ANY( plPythonFileMod, plMultiModifier );

    plPythonSDLModifier* GetSDLMod() { return fSDLMod; }
    bool WasLocalNotify() { return fLocalNotify; }
    plPipeline* GetPipeline() { return fPipe; }
    virtual void SetSourceFile(const ST::string& filename);
    virtual int getPythonOutput(std::string* line);
    virtual void ReportError();
    virtual void DisplayPythonOutput();
    static void SetAtConvertTime() { fAtConvertTime=true; }
    virtual bool AmIAttachedToClone() { return fAmIAttachedToClone; }

    virtual void AddToNotifyList(plKey pKey) { fReceivers.Append(pKey); }
    virtual int32_t NotifyListCount() { return fReceivers.Count(); }
    virtual plKey GetNotifyListItem(int32_t i) { return fReceivers[i]; }

    virtual void AddParameter(plPythonParameter param) { fParameters.Append(param); }
    virtual int32_t GetParameterListCount() { return fParameters.Count(); }
    virtual plPythonParameter GetParameterItem(int32_t i) { return fParameters[i]; }
    
    virtual void AddTarget(plSceneObject* sobj);
    virtual void RemoveTarget(plSceneObject* so); 

    virtual void EnableControlKeyEvents();
    virtual void DisableControlKeyEvents();
    
    virtual bool MsgReceive(plMessage* msg);

    virtual void Read(hsStream* stream, hsResMgr* mgr);
    virtual void Write(hsStream* stream, hsResMgr* mgr);

    // array of matching Python instance where the functions are, if defined
    PyObject* fPyFunctionInstances[kfunc_lastone];
    // array of the names of the standard functions that can be called
    static const char* fFunctionNames[];

    // The konstant hard-coded name to be used for all global pythonFileMods
    static ST::string kGlobalNameKonstant;

    // API for processing discarded keys as the deafult key catcher
    void    HandleDiscardedKey( plKeyEventMsg *msg );
};

#endif // _plPythonFileMod_h
