// svcdrawer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stdio.h>
#include <Windows.h>
#include <ctime>
#include <cstring>
#include <string>

constexpr wchar_t kServiceName[] = L"ServiceDrawer";

//Global variable
SERVICE_STATUS  g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = NULL;

void logErrorEvent(const wchar_t* operation, DWORD errorCode) {
    HANDLE hEventLog = RegisterEventSource(NULL, kServiceName);
    if (hEventLog) {
        wchar_t message[256] = { 0 };
        swprintf_s(message, L"%s failed with error %lu", operation, errorCode);
        const wchar_t* messages[] = { message };
        ReportEvent(hEventLog, EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0, messages, NULL);
        DeregisterEventSource(hEventLog);
    }
}

bool updateServiceStatus(DWORD state, DWORD win32ExitCode, DWORD waitHint, DWORD controlsAccepted) {
    static DWORD checkPoint = 1;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = state;
    g_ServiceStatus.dwWin32ExitCode = win32ExitCode;
    g_ServiceStatus.dwWaitHint = waitHint;
    g_ServiceStatus.dwControlsAccepted = controlsAccepted;

    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED) {
        g_ServiceStatus.dwCheckPoint = 0;
    }
    else {
        g_ServiceStatus.dwCheckPoint = checkPoint++;
    }

    if (!SetServiceStatus(g_StatusHandle, &g_ServiceStatus)) {
        logErrorEvent(L"SetServiceStatus", GetLastError());
        return false;
    }

    return true;
}

void WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
            updateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 10000, 0);
            SetEvent(g_ServiceStopEvent);
        }
        break;
    default:
        break;
    }
}

void eventLogger(std::wstring eventmessage) {
    HANDLE hEventLog = RegisterEventSource(NULL, kServiceName);
    if (hEventLog) {
        // Get system time
        std::time_t now = std::time(nullptr);
        char buf[100] = { 0 };
        struct tm localTime;
        if (localtime_s(&localTime, &now) == 0) {
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localTime);
        }

        std::wstring message = L"ServiceDrawer is running. at ";
        message += std::wstring(buf, buf + std::strlen(buf));
        message += L" - " + eventmessage;


        const wchar_t* messages[] = { message.c_str()};
        ReportEvent(hEventLog, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, messages, NULL);
        DeregisterEventSource(hEventLog);
    }
}


void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    g_StatusHandle = RegisterServiceCtrlHandler((LPWSTR)kServiceName, ServiceCtrlHandler);
    if (g_StatusHandle == NULL) {
        logErrorEvent(L"RegisterServiceCtrlHandler", GetLastError());
        return;
    }

    if (!updateServiceStatus(SERVICE_START_PENDING, NO_ERROR, 10000, 0)) {
        return;
    }

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL) {
        DWORD errorCode = GetLastError();
        logErrorEvent(L"CreateEvent", errorCode);
        updateServiceStatus(SERVICE_STOPPED, errorCode, 0, 0);
        return;
    }

    if (!updateServiceStatus(SERVICE_RUNNING, NO_ERROR, 0, SERVICE_ACCEPT_STOP)) {
        CloseHandle(g_ServiceStopEvent);
        g_ServiceStopEvent = NULL;
        return;
    }

    // Worker loop
    while (WaitForSingleObject(g_ServiceStopEvent, 10000) != WAIT_OBJECT_0) {
        // Do background work here
        eventLogger(L"Hello testing");
        Sleep(30000);
    }

    CloseHandle(g_ServiceStopEvent);
    g_ServiceStopEvent = NULL;

    updateServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0);
}

//read textfile
void readTextFile(const std::wstring& filePath) {
	FILE* file;
	errno_t err = _wfopen_s(&file, filePath.c_str(), L"r");
	if (err != 0 || file == nullptr) {
		logErrorEvent(L"Failed to open file", err);
		return;
	}

	wchar_t buffer[256];
	while (fgetws(buffer, sizeof(buffer) / sizeof(wchar_t), file)) {
		// Process the line read from the file
		eventLogger(std::wstring(buffer));
	}

	fclose(file);
}

//create process

int createNewProcess(std::string apppath, std::string commandlines) {
   // LPCSTR applicationName = "C:\\Windows\\System32\\notepad.exe"; // Change this to the desired application path

    //printf("Starting Process: %s\n", "test");
    std::string commandLine = apppath + " " + commandlines;
    printf("Command Line: %s\n", commandLine.c_str());

    char* commandLineCStr = new char[commandLine.length() + 1];
    std:strcpy_s(commandLineCStr, commandLine.length() + 1, commandLine.c_str());
    //std::strcpy_s(commandLineCStr, commandLine.c_str());

    //create JObObject to attach parent and child process whenever any behavior happened to parent such as suspend/resume/exit
        // Create a job object
    HANDLE hJob = CreateJobObject(NULL, NULL);
    if (hJob == NULL) {
        std::cerr << "CreateJobObject failed (" << GetLastError() << ")." << std::endl;
        return 1;
    }

    // Configure the job so child processes die when the job is closed
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = {};
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

    if (!SetInformationJobObject(
        hJob,
        JobObjectExtendedLimitInformation,
        &jeli,
        sizeof(jeli)
    )) {
        std::cerr << "SetInformationJobObject failed (" << GetLastError() << ")." << std::endl;
        CloseHandle(hJob);
        return 1;
    }


    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION pi;

    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    ZeroMemory(&pi, sizeof(pi));


    if (!CreateProcessA(
		NULL,   // Application name
        commandLineCStr,              // Command line arguments
		NULL,              // Process handle not inheritable
		NULL,              // Thread handle not inheritable
		FALSE,             // Set handle inheritance to FALSE
		0,                 // No creation flags
		NULL,              // Use parent's environment block
		NULL,              // Use parent's starting directory 
		&startupInfo,      // Pointer to STARTUPINFO structure
		&pi)               // Pointer to PROCESS_INFORMATION structure
	) {

		DWORD errorCode = GetLastError();
		logErrorEvent(L"CreateProcessA", errorCode);
	}
	else {

        // Assign the process to the job
        if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
            std::cerr << "AssignProcessToJobObject failed (" << GetLastError() << ")." << std::endl;
        }

        // Resume the process
        ResumeThread(pi.hThread);

        std::cout << "Process started and assigned to job." << std::endl;


        //line so that the process is created and wait for it to finish
        WaitForSingleObject(pi.hProcess, INFINITE);

		// Successfully created the process. Close handles.
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
    return 0;

}

int main()
{
    //std::cout << "Hello World!\n";
    printf("hello world %s \n", "test");
    printf("This application is demo to run as a Windows service. To install the service, use the following command:\n");

    /*
    SERVICE_TABLE_ENTRY ServiceTable[] = {
    {(LPWSTR)kServiceName, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
    {NULL, NULL}
    };

    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        DWORD errorCode = GetLastError();
        if (errorCode != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            logErrorEvent(L"StartServiceCtrlDispatcher", errorCode);
            return static_cast<int>(errorCode);
        }
    }
    */

    createNewProcess("C:\\Windows\\System32\\notepad.exe", "C:\\Users\\MeijSandbox\\Documents\\testgitbucetCLI.txt");
    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
