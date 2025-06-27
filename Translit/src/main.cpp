// Copyright (c) 2023, Eugene Gershnik
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Translit.h"

#include <Common/err.h>
#include <Common/RegKey.h>
#include <Common/win32api.h>
#include <Translit/Identifiers.h>


static constexpr const GUID * g_supportedCategories[] = {
    &GUID_TFCAT_TIP_KEYBOARD,
    &GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    &GUID_TFCAT_TIPCAP_UIELEMENTENABLED, 
    &GUID_TFCAT_TIPCAP_SECUREMODE,
    &GUID_TFCAT_TIPCAP_COMLESS,
    &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, 
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
};

static std::atomic<ULONG> g_moduleRefCount;

ULONG Module::add_ref() noexcept
    { return ++g_moduleRefCount; }
ULONG Module::sub_ref() noexcept
    { return --g_moduleRefCount; }


template<class Func>
requires(std::is_invocable_v<Func, IUnknown *>)
class ClassFactory final : public ComStaticObject<ClassFactory<Func>, IClassFactory>
{
public:
    constexpr ClassFactory(Func && func): m_func(std::forward<Func>(func))
    {}

    // IClassFactory methods
    STDMETHODIMP CreateInstance(_In_opt_ IUnknown* pUnkOuter, _In_ REFIID riid, _COM_Outptr_ void ** ppvObj) {
        COM_PROLOG
            if (!ppvObj)
                return E_INVALIDARG;
            if (pUnkOuter && riid != __uuidof(IUnknown)) {
                *ppvObj = nullptr;
                return E_NOINTERFACE;
            }
            auto obj = m_func(pUnkOuter);
            return obj->QueryInterface(riid, ppvObj);
        COM_EPILOG
    }
    STDMETHODIMP LockServer(BOOL fLock) {
        if (fLock)
            Module::add_ref();
        else
            Module::sub_ref();
        
        return S_OK;
    }

private:
    Func m_func;
};

static auto g_TranslitFactory = ClassFactory([](IUnknown * pUnkOuter) {
    if (pUnkOuter)
        throwHresult(CLASS_E_NOAGGREGATION);
    return refcnt_attach(new Translit());
});

static bool checkIfAllowed() {
    auto exeName = getModuleFileName(0);
    if (auto parts = exeName.partition_at_last(U'\\')) {
        exeName = parts->second;
    }

    if (compare_no_case(exeName, S("TranslitSettings.exe")) == std::strong_ordering::equal ||
        compare_no_case(exeName, S("msiexec.exe")) == std::strong_ordering::equal ||
        compare_no_case(exeName, S("regsvr32.exe")) == std::strong_ordering::equal)
        return true;

    std::error_code ec;
    auto appKey = RegKey::currentUser->open(L"Software\\Translit", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, ec);
    if (ec)
        return true;

    if (auto whitelist = appKey->getMultiStringValue(L"Whitelist", ec); !ec) {
        bool inWhitelist = false;
        for(const wchar_t * name = whitelist.data(); *name; name += wcslen(name) + 1) {
            if (_wcsicmp(exeName.w_str(), name) == 0) {
                inWhitelist = true;
                break;
            }
        }
        if (!inWhitelist)
            return false;
    }
    
    if (auto blacklist = appKey->getMultiStringValue(L"Blacklist", ec); !ec) {
        for(const wchar_t * name = blacklist.data(); *name; name += wcslen(name) + 1) {
            if (_wcsicmp(exeName.w_str(), name) == 0) {
                return false;
            }
        }
    }

    return true;
}

BOOL WINAPI DllMain([[maybe_unused]] HINSTANCE hInstance, [[maybe_unused]] DWORD dwReason, [[maybe_unused]] LPVOID pvReserved) {

    if (dwReason == DLL_PROCESS_ATTACH) {
        return checkIfAllowed();
    }
	return true;
}


_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ void** ppv) {


    if (rclsid == g_translitClassId) {
        auto factory = com_retain(&g_TranslitFactory);
        return factory->QueryInterface(riid, ppv);
    }
	
    *ppv = nullptr;
	return CLASS_E_CLASSNOTAVAILABLE;
}

__control_entrypoint(DllExport)
STDAPI DllCanUnloadNow() {
    return g_moduleRefCount.load() == 0 ?  S_OK : S_FALSE;
}

static void registerProfiles(const sys_string & filename, std::set<sys_string> * enabledLanguages = nullptr) {
    
    auto inputProcessorProfiles = comCreate<ITfInputProcessorProfiles>(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER);
    
    comTest(inputProcessorProfiles->Register(g_translitServiceId));

    for (auto & profile: getProfiles()) {
        comTest(inputProcessorProfiles->AddLanguageProfile(g_translitServiceId,
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            *profile.id,
            profile.description,
            ULONG(-1),
            filename.w_str(),
            ULONG(filename.storage_size()),
            ULONG(-profile.icondId)));

        bool enabled = (enabledLanguages && enabledLanguages->contains(profile.lang));

        comTest(inputProcessorProfiles->EnableLanguageProfileByDefault(g_translitServiceId, 
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            *profile.id,
            enabled));
        comTest(inputProcessorProfiles->EnableLanguageProfile(g_translitServiceId, 
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            *profile.id,
            enabled));
    }

    auto categoryMgr = comCreate<ITfCategoryMgr>(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER);

    for (auto cat: g_supportedCategories) {
        comTest(categoryMgr->RegisterCategory(g_translitServiceId, *cat, g_translitServiceId));
    }
}

static void unregisterProfiles() {

    auto categoryMgr = comCreate<ITfCategoryMgr>(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER);
    
    auto threadMgr = comCreate<ITfThreadMgr>(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER);

    auto inputProcessorProfiles= comCreate<ITfInputProcessorProfiles>(CLSID_TF_InputProcessorProfiles, nullptr,  CLSCTX_INPROC_SERVER);

    for (auto cat: g_supportedCategories) {
        categoryMgr->UnregisterCategory(g_translitServiceId, *cat, g_translitServiceId);
    }

    for (auto & profile: getProfiles()) {
        inputProcessorProfiles->EnableLanguageProfile(g_translitServiceId, 
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            *profile.id,
            false);
        inputProcessorProfiles->EnableLanguageProfileByDefault(g_translitServiceId, 
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            *profile.id,
            false);
        inputProcessorProfiles->RemoveLanguageProfile(g_translitServiceId, 
            MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
            *profile.id);
    }

    inputProcessorProfiles->Unregister(g_translitServiceId);

}

STDAPI DllRegisterServer() {
    COM_PROLOG

        sys_string clsidKeyPath = S("{") + uuid(g_translitClassId).to_chars() + S("}");
        auto filename = getModuleFileName(Module::handle());

        auto classes_key = RegKey::classesRoot->open(L"CLSID", 0, KEY_WRITE);
        auto class_key = classes_key->create(clsidKeyPath.w_str(), REG_OPTION_NON_VOLATILE, KEY_WRITE);
        class_key->setStringValue(nullptr, L"Translit Text Service");
        auto inproc_key = class_key->create(L"InProcServer32", REG_OPTION_NON_VOLATILE, KEY_WRITE);
        inproc_key->setStringValue(nullptr, filename.w_str());
        inproc_key->setStringValue(L"ThreadingModel", L"Apartment");

        registerProfiles(filename);

    return S_OK;
    COM_EPILOG
}

STDAPI DllUnregisterServer() {
    COM_PROLOG

        unregisterProfiles();
        
        sys_string clsidKeyPath = S("{") + uuid(g_translitClassId).to_chars() + S("}");

        auto classes_key = RegKey::classesRoot->open(L"CLSID", 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE | DELETE);
        AllowedErrors<ERROR_FILE_NOT_FOUND> allowed;
        classes_key->deleteTree(clsidKeyPath.w_str(), allowed);

        return S_OK;
    COM_EPILOG
}

template<class... Args>
static void msiLog(MSIHANDLE hInstall, std::format_string<Args...> fmt, Args &&... args) {
    auto str = sys_string::std_format(fmt, std::forward<Args>(args)...);
    PMSIHANDLE hRecord = MsiCreateRecord(1);
    MsiRecordSetString(hRecord, 0, L"Translit: [1]");
    MsiRecordSetString(hRecord, 1, str.w_str());
    MsiProcessMessage(hInstall, INSTALLMESSAGE_INFO, hRecord);
}

static sys_string getMsiProperty(MSIHANDLE hInstall) {
    std::vector<wchar_t> buf = {L'0'};
    for ( ; ; ) {
        auto size = DWORD(buf.size());
        UINT res = MsiGetProperty(hInstall, L"CustomActionData", buf.data(), &size);
        buf.resize(size + 1);
        if (res == ERROR_SUCCESS)
            break;
        if (res != ERROR_MORE_DATA)
            throwWinError(res);
    }
    return sys_string(buf.data(), buf.size() - 1);
}

extern "C" UINT __stdcall RegisterProfilesAction([[maybe_unused]] MSIHANDLE hInstall) {
    try {
        auto data = getMsiProperty(hInstall);
        msiLog(hInstall, "Langs to enable: `{}`", data);

        std::set<sys_string> enabledLanguages;
        data.split(std::inserter(enabledLanguages, enabledLanguages.end()), U';', size_t(-1));

        HRESULT hres = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hres) && hres != RPC_E_CHANGED_MODE)
            throwHresult(hres);

        auto filename = getModuleFileName(Module::handle());
        registerProfiles(filename, &enabledLanguages);

        return ERROR_SUCCESS;

    } catch(std::system_error & ex) {
        msiLog(hInstall, "RegisterProfilesAction: error: 0x{:08X}, {}", DWORD(ex.code().value()), ex.what());
    } catch(std::exception & ex) {
        msiLog(hInstall, "RegisterProfilesAction: error: {}", ex.what());
    }
    return ERROR_INSTALL_FAILURE;
}

extern "C" UINT __stdcall UnregisterProfilesAction([[maybe_unused]] MSIHANDLE hInstall) {
    try {

        HRESULT hres = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(hres) && hres != RPC_E_CHANGED_MODE)
            throwHresult(hres);

        unregisterProfiles();

        return ERROR_SUCCESS;

    } catch(std::system_error & ex) {
        msiLog(hInstall, "UnregisterProfilesAction: error: 0x{:08X}, {}", DWORD(ex.code().value()), ex.what());
    } catch(std::exception & ex) {
        msiLog(hInstall, "UnregisterProfilesAction: error: {}", ex.what());
    }
    return ERROR_INSTALL_FAILURE;

}

