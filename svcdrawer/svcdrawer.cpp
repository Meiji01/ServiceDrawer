// svcdrawer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stdio.h>
#include <Windows.h>
#include <shellapi.h>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>

#include "logger.h"

#pragma comment(lib, "Shell32.lib")

constexpr wchar_t kServiceName[] = L"ServiceDrawer";

//Global variable
SERVICE_STATUS  g_ServiceStatus = { 0 };
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = NULL;

std::wstring g_appname;
std::wstring g_appparam;

// Tracks the job object of the currently running child process so the stop
// handler can force-kill it if it is hung/locked and blocking the read loop.
HANDLE g_ChildJob = NULL;
SRWLOCK g_ChildJobLock = SRWLOCK_INIT;

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

        std::wstring message = L"Message: ";
        message += std::wstring(buf, buf + std::strlen(buf));
        message += L" - " + eventmessage;


        const wchar_t* messages[] = { message.c_str() };
        ReportEvent(hEventLog, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, messages, NULL);
        DeregisterEventSource(hEventLog);
    }
}

void terminateTrackedChildJob() {
    HANDLE childJob = NULL;

    AcquireSRWLockExclusive(&g_ChildJobLock);
    childJob = g_ChildJob;
    g_ChildJob = NULL;
    ReleaseSRWLockExclusive(&g_ChildJobLock);

    if (childJob != NULL) {
        TerminateJobObject(childJob, ERROR_PROCESS_ABORTED);
        eventLogger(L"Child process terminated due to service stop request.");
    }
}

void WINAPI ServiceCtrlHandler(DWORD CtrlCode) {
    switch (CtrlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState == SERVICE_RUNNING) {
            updateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 10000, 0);

            // Wake the worker immediately so the stop path is observed without waiting
            // for a child pipe read or process handle to block the service loop.
            if (g_ServiceStopEvent != NULL) {
                SetEvent(g_ServiceStopEvent);
            }

            // Force-kill a hung/locked child so the child lifecycle can unwind.
            terminateTrackedChildJob();
        }
        break;
    default:
        break;
    }
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


void getCommandLineArguments(int argc, wchar_t* argv[], std::wstring& appname, std::wstring& appparam, bool& isService) {
    if (argc > 1) {
        appname = argv[1];

        for (int i = 2; i < argc; ++i) {
            if (i > 2) {
                appparam += L" "; // Add space between parameters
            }
            appparam += argv[i];
        }

    }
    else {
        // If no arguments are provided, use default values
        appname = L"cmd /c /u";
        appparam = L"C:\\Jenkins\\runagent.bat";
    }

    // Check if the application is running as a service
    isService = (GetConsoleWindow() == NULL);
}

//create process

int createNewProcess(std::wstring appname, std::wstring commandlines) {
   // LPCSTR applicationName = "C:\\Windows\\System32\\notepad.exe"; // Change this to the desired application path

    //printf("Starting Process: %s\n", "test");
    std::wstring commandLine = appname + L" " + commandlines;
    wprintf(L"\nCommand Line: %s\n", commandLine.c_str());

    std::vector<char> commandLineBuffer(commandLine.begin(), commandLine.end());
    commandLineBuffer.push_back('\0');
    char* commandLineCStr = commandLineBuffer.data();

    // create job object to attach parent and child process whenever any behavior happened to parent such as suspend/resume/exit
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


    //Handles the inheritance for the child process
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;  // Allow child to inherit handle
    sa.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe = NULL;
    HANDLE hWritePipe = NULL;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        std::cerr << "CreatePipe failed." << std::endl;
        CloseHandle(hJob);
        return 1;
    }

    // Ensure the read handle is not inherited
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA startupInfo;
    PROCESS_INFORMATION pi;

    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);
    ZeroMemory(&pi, sizeof(pi));

    startupInfo.hStdOutput = hWritePipe;
    startupInfo.hStdError = hWritePipe;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES; // required for hStdOutput/hStdError to take effect

    BOOL processCreated = CreateProcessA(
        NULL,
        commandLineCStr,
        NULL,
        NULL,
        TRUE, // must inherit handles so the child can write into hWritePipe
        0,
        NULL,
        NULL,
        &startupInfo,
        &pi
    );

    if (!processCreated) {
        DWORD errorCode = GetLastError();
        logErrorEvent(L"CreateProcessA", errorCode);
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        CloseHandle(hJob);
        return 1;
    }

    // Assign the process to the job
    if (!AssignProcessToJobObject(hJob, pi.hProcess)) {
        std::cerr << "AssignProcessToJobObject failed (" << GetLastError() << ")." << std::endl;
    }

    // Publish the job so a service stop request can force-kill a locked child.
    AcquireSRWLockExclusive(&g_ChildJobLock);
    g_ChildJob = hJob;
    ReleaseSRWLockExclusive(&g_ChildJobLock);

    // Resume the process
    ResumeThread(pi.hThread);

    std::cout << "Process started and assigned to job." << std::endl;

    // Close the write end in the parent so we can read
    CloseHandle(hWritePipe);

    HANDLE waitHandles[3] = { pi.hProcess, hReadPipe, g_ServiceStopEvent };
    DWORD waitCount = 2;
    if (g_ServiceStopEvent != NULL) {
        waitCount = 3;
    }

    // Read output until the process exits or the service is stopping.
    while (true) {
        DWORD waitResult = WaitForMultipleObjects(waitCount, waitHandles, FALSE, 100);

        if (waitResult == WAIT_TIMEOUT) {
            continue;
        }

        if (waitResult == WAIT_FAILED) {
            break;
        }

        if (waitResult == WAIT_OBJECT_0) {
            break;
        }

        if (g_ServiceStopEvent != NULL && waitResult == WAIT_OBJECT_0 + 2) {
            terminateTrackedChildJob();
            break;
        }

        if (waitResult == WAIT_OBJECT_0 + 1) {
            char buffer[4096];
            DWORD available = 0;
            while (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &available, NULL) && available > 0) {
                DWORD bytesRead = 0;
                if (!ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) || bytesRead == 0) {
                    break;
                }

                buffer[bytesRead] = '\0';
                std::cout << buffer;
                writeConsoleOutputToLog(buffer);
            }
        }
    }

    // Ensure we don't wait forever if the child is hung or the service is stopping.
    DWORD waitResult = WaitForSingleObject(pi.hProcess, 5000);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, ERROR_PROCESS_ABORTED);
        WaitForSingleObject(pi.hProcess, 5000);
    }

    // Unpublish the job before closing it so the stop handler can no longer touch it.
    terminateTrackedChildJob();

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    CloseHandle(hJob);
    return 0;

}

//void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
void WINAPI ServiceMain(int argc, wchar_t* argv[]) {
    eventLogger(L"ServiceMain: Service is starting.");
    writeConsoleOutputToLog("ServiceMain: Service is starting.\n");
    //writeConsoleOutputToLog("Windows Service Argument count:" + std::to_string(argc));


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
        //eventLogger(L"Hello testing");
        //Sleep(30000);

        bool isService = true;
        //getCommandLineArguments(argc, argv, appname, appparam, isService);

        //wprintf(L"Arguments: %s\n", argtext.c_str());
        wprintf(L"Application name: %s\n", g_appname.c_str());
        wprintf(L"Application parameters: %s\n", g_appparam.c_str());
        eventLogger(L"Service Drawer Attempt to start\nApplication name:" + g_appname + L"\nWith parameters: " + g_appparam);
        createNewProcess(g_appname, g_appparam);
        break;

    }
    eventLogger(L"Service is stopping.");
    CloseHandle(g_ServiceStopEvent);
    g_ServiceStopEvent = NULL;

    updateServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0);
}

int wmain(int argc, wchar_t* argv[])
{
    //std::cout << "Hello World!\n";
    //printf("hello world %s \n", "test");
    printf("Service Drawer 1.0");
    printf("This application is a Windows Service Wrapper.\n");
    printf("Parameter count:%d\n", argc-1);

    //debug param count
    writeConsoleOutputToLog("Console Parameter count:" + std::to_string(argc-1) + "\n");
    
    bool isService = false;
    
    //populate the appname and appparam and modify the global variables
    getCommandLineArguments(argc, argv, g_appname, g_appparam, isService);


    // Reliable service detection: SCM connects here only when launched as a service.

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
        // Not started by the SCM -> running interactively from the command line.
        printf("Running interactively from the command line...\n");
        isService = false;
    }
    else {
        // StartServiceCtrlDispatcher blocked here while running as a service; it has now stopped.
        return 0;
    }

    //writeConsoleOutputToLog("Running interactively from the command line...");

    if (argc > 1) {
        printf("Arguments:\n");

        
        for (DWORD i = 0; i < argc; ++i) {

            std::wstring argument(argv[i]);
            wprintf(L"Argument[%d]: %s\n", i, argument.c_str());

        }

        
        //wprintf(L"Arguments: %s\n", argtext.c_str());
        wprintf(L"Application name: %s\n", g_appname.c_str());
        wprintf(L"Application parameters: %s\n", g_appparam.c_str());

        //appname = std::string narrow(argtext.begin(),argext.end());
    }
    else {
        printf("*\n*\nNo parameter, will use debug default application!");
        //appname = defaultapp;
        g_appname = L"cmd";
        g_appparam = std::wstring(L"/u /c ") + g_appparam;
    }


    //createNewProcess("cmd /u /c", "C:\\Users\\MeijSandbox\\source\\repos\\svcdrawer\\ServiceDrawer\\svcdrawer\\test\\runConsole.bat");
    createNewProcess(g_appname, g_appparam);


    g_appname.clear();
    g_appparam.clear();

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
