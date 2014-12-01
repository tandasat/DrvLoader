#include "stdafx.h"


#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>


// C/C++ standard headers
// Other external headers
// Windows headers
// Original headers


////////////////////////////////////////////////////////////////////////////////
//
// macro utilities
//


////////////////////////////////////////////////////////////////////////////////
//
// constants and macros
//


////////////////////////////////////////////////////////////////////////////////
//
// types
//


////////////////////////////////////////////////////////////////////////////////
//
// prototypes
//


namespace {


bool AppMain(
    __in int Argc,
    __in TCHAR* const Argv[]);

bool IsServiceInstalled(
    __in LPCTSTR ServiceName);

bool LoadDriver(
    __in LPCTSTR ServiceName,
    __in LPCTSTR DriverFile);

bool UnloadDriver(
    __in LPCTSTR ServiceName);

void PrintErrorMessage(
    __in const char* Message);

std::string GetErrorMessage(
    __in DWORD ErrorCode);


} // End of namespace {unnamed}


////////////////////////////////////////////////////////////////////////////////
//
// variables
//


////////////////////////////////////////////////////////////////////////////////
//
// implementations
//

int _tmain(int argc, _TCHAR* argv[])
{
    int result = EXIT_FAILURE;
    try
    {
        if (AppMain(argc, argv))
        {
            result = EXIT_SUCCESS;
        }
    }
    catch (std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    catch (...)
    {
        std::cout << "Unknown Exception." << std::endl;
    }
    return result;
}


namespace {


bool AppMain(
    __in int Argc,
    __in TCHAR* const Argv[])
{
    if (Argc != 2)
    {
        std::cout
            << "usage:\n"
            << "    >DrvLoader.exe <DriverFile>\n"
            << std::endl;
        return false;
    }

    const auto driverName = Argv[1];

    TCHAR fullPath[MAX_PATH];
    if (!::PathSearchAndQualify(driverName, fullPath, _countof(fullPath)))
    {
        PrintErrorMessage("PathSearchAndQualify failed");
        return false;
    }

    if (!::PathFileExists(fullPath))
    {
        PrintErrorMessage("PathFileExists failed");
        return false;
    }

    // Create a service name
    TCHAR serviceName[MAX_PATH];
    if (!SUCCEEDED(::StringCchCopy(serviceName, _countof(serviceName),
        fullPath)))
    {
        PrintErrorMessage("StringCchCopy failed");
        return false;
    }

    ::PathRemoveExtension(serviceName);
    ::PathStripPath(serviceName);

    if (IsServiceInstalled(serviceName))
    {
        // Uninstall the service when it has already been installed
        if (!UnloadDriver(serviceName))
        {
            PrintErrorMessage("UnloadDriver failed");
            return false;
        }
        std::cout << "unload successfully" << std::endl;
    }
    else
    {
        // Install the service when it has not been installed yet
        if (!LoadDriver(serviceName, fullPath))
        {
            if (::GetLastError() == ERROR_INVALID_PARAMETER)
            {
                std::cout << "the driver was executed and unloaded." << std::endl;
                return true;
            }
            PrintErrorMessage("LoadDriver failed");
            return false;
        }
        std::cout << "load successfully" << std::endl;
    }
    return true;
}


// Returns true when a specified service has been installed.
bool IsServiceInstalled(
    __in LPCTSTR ServiceName)
{
    auto scmHandle = std::experimental::unique_resource(
        ::OpenSCManager(nullptr, nullptr, GENERIC_READ), ::CloseServiceHandle);
    return (FALSE != ::CloseServiceHandle(
        ::OpenService(scmHandle.get(), ServiceName, GENERIC_READ)));
}


// Loads a driver file as a file system filter driver.
bool LoadDriver(
    __in LPCTSTR ServiceName,
    __in LPCTSTR DriverFile)
{
    static const TCHAR INSTANCE_NAME_T[] = _T("instance_name");
    static const TCHAR ALTITUDE[] = _T("370040");

    // Create registry values for a file system driver
    TCHAR fsRegistry[260];
    if (!SUCCEEDED(::StringCchPrintf(fsRegistry, _countof(fsRegistry),
        _T("SYSTEM\\CurrentControlSet\\Services\\%s\\Instances"), ServiceName)))
    {
        return false;
    }

    HKEY keyNative = nullptr;
    if (ERROR_SUCCESS != ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, fsRegistry, 0,
        nullptr, 0, KEY_ALL_ACCESS, nullptr, &keyNative, nullptr))
    {
        return false;
    }

    auto key = std::experimental::unique_resource(std::move(keyNative), 
                                                  ::RegCloseKey);
    if (ERROR_SUCCESS != ::RegSetValueEx(key.get(), _T("DefaultInstance"), 0,
        REG_SZ, reinterpret_cast<const BYTE*>(INSTANCE_NAME_T),
        sizeof(INSTANCE_NAME_T)))
    {
        return false;
    }

    ::StringCchCat(fsRegistry, _countof(fsRegistry), _T("\\"));
    if (!SUCCEEDED(::StringCchCat(fsRegistry, _countof(fsRegistry),
        INSTANCE_NAME_T)))
    {
        return false;
    }

    HKEY keySubNative = nullptr;
    if (ERROR_SUCCESS != ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, fsRegistry, 0,
        nullptr, 0, KEY_ALL_ACCESS, nullptr, &keySubNative, nullptr))
    {
        return false;
    }

    auto keySub = std::experimental::unique_resource(std::move(keySubNative), 
                                                     ::RegCloseKey);
    if (ERROR_SUCCESS != ::RegSetValueEx(keySub.get(), _T("Altitude"), 0,
        REG_SZ, reinterpret_cast<const BYTE*>(ALTITUDE), sizeof(ALTITUDE)))
    {
        return false;
    }

    DWORD regValue = 0;
    if (ERROR_SUCCESS != ::RegSetValueEx(keySub.get(), _T("Flags"), 0,
        REG_DWORD, reinterpret_cast<const BYTE*>(&regValue), sizeof(regValue)))
    {
        return false;
    }

    auto scmHandle = std::experimental::unique_resource(::OpenSCManager(
        nullptr, nullptr, SC_MANAGER_CREATE_SERVICE), ::CloseServiceHandle);
    if (!scmHandle)
    {
        return false;
    }

    auto serviceHandle = std::experimental::unique_resource(
        ::CreateService(scmHandle.get(), ServiceName, ServiceName,
        SERVICE_ALL_ACCESS, SERVICE_FILE_SYSTEM_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, DriverFile,
        _T("FSFilter Activity Monitor"), nullptr, _T("FltMgr"), nullptr,
        nullptr), ::CloseServiceHandle);
    if (!serviceHandle)
    {
        return false;
    }

    // Start the service
    SERVICE_STATUS status = {};
    if (::StartService(serviceHandle.get(), 0, nullptr))
    {
        while (::QueryServiceStatus(serviceHandle.get(), &status))
        {
            if (status.dwCurrentState != SERVICE_START_PENDING)
            {
                break;
            }

            ::Sleep(500);
        }
    }

    if (status.dwCurrentState != SERVICE_RUNNING)
    {
        ::DeleteService(serviceHandle.get());
        return false;
    }
    return true;
}


// Unloads a driver and deletes its service.
bool UnloadDriver(
    __in LPCTSTR ServiceName)
{
    auto scmHandle = std::experimental::unique_resource(::OpenSCManager(
        nullptr, nullptr, SC_MANAGER_CONNECT), ::CloseServiceHandle);
    if (!scmHandle)
    {
        return false;
    }

    auto serviceHandle = std::experimental::unique_resource(::OpenService(
        scmHandle.get(), ServiceName, 
        DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS), ::CloseServiceHandle);
    if (!serviceHandle)
    {
        return false;
    }

    ::DeleteService(serviceHandle.get());

    // Stop the service
    SERVICE_STATUS status = {};
    if (::ControlService(serviceHandle.get(), SERVICE_CONTROL_STOP, &status))
    {
        while (::QueryServiceStatus(serviceHandle.get(), &status))
        {
            if (status.dwCurrentState != SERVICE_START_PENDING)
                break;

            Sleep(500);
        }
    }

    TCHAR fsRegistry[260];
    if (SUCCEEDED(StringCchPrintf(fsRegistry, _countof(fsRegistry),
        _T("SYSTEM\\CurrentControlSet\\Services\\%s\\"), ServiceName)))
    {
        ::SHDeleteKey(HKEY_LOCAL_MACHINE, fsRegistry);
    }

    return (status.dwCurrentState == SERVICE_STOPPED);
}


void PrintErrorMessage(
    __in const char* Message)
{
    const auto errorCode = ::GetLastError();
    const auto errorMessage = GetErrorMessage(errorCode);
    ::fprintf_s(stderr, "%s : %lu(0x%p) : %s\n", Message, errorCode,
        errorCode, errorMessage.c_str());
}


std::string GetErrorMessage(
    __in DWORD ErrorCode)
{
    char* messageNaked = nullptr;
    if (!::FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
        ErrorCode, LANG_USER_DEFAULT, reinterpret_cast<LPSTR>(&messageNaked), 
        0, nullptr))
    {
        return "";
    }
    auto message = std::experimental::unique_resource(std::move(messageNaked),
                                                     ::LocalFree);

    const auto length = ::strlen(message.get());
    if (!length)
    {
        return "";
    }

    if (message.get()[length - 2] == '\r')
    {
        message.get()[length - 2] = '\0';
    }
    return message.get();
}


} // End of namespace {unnamed}

