// createservice.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
using namespace std;

#include "writer.h"
#include <windows.h>

int main()
{

    cout << "Service Creator Tool\n" << flush;
    cout << "This tool will create a service install and uninstall batch file for you.\n" << flush;
    cout << "**WARNING**: This tool will not create the service for you, it will only create the batch files to do so. You must run the install batch file as administrator to create the service.\n"
    << flush;
    cout << "" << flush;
    cout << "Please enter the following information:\n" << flush;



    //get the current working directory
    char buffer[MAX_PATH];
    //GetCurrentDirectoryA(MAX_PATH, buffer);
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    //remove the filename
    //int size=strlen(buffer);
    std::string exepath(buffer);

    size_t pos = exepath.find_last_of("\\/");
    std::string workingpath = exepath.substr(0, pos) + "\\";



    //std::string workingpath(buffer);
    //workingpath += "\\"; // Add a backslash to the end of the path


    //string variables
    //std::string serviceName;
    std::string serviceDisplayName;
    std::string serviceDescription;
    std::string serviceExecutablePath = workingpath + "svcdrawer.exe";
    std::string serviceParameters;
    std::string finalServiceString;
    std::string sha256hex;

    //need to input the text to write to the file and the file name
    //std::string currentvalue = "";
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    // Save current attributes
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(hConsole, &consoleInfo);
    WORD saved_attributes = consoleInfo.wAttributes;

    do {
        if (serviceDisplayName.empty()) {
            cout << "Enter service name: " << flush;
            getline(cin, serviceDisplayName);
            continue;
        }
        else if (serviceDescription.empty()) {
            cout << "Enter service description: " << flush;
            //cin >> serviceDescription;
            getline(cin, serviceDescription);
            continue;
        }
        else if (serviceParameters.empty()) {
            cout << "Enter service parameters (App Path + params)\n**For .bat targets, use " << flush;

            SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            cout << "cmd /u /c C:\\...\\<batchfilename>.bat: " << flush;
            
            // Restore original attributes
            SetConsoleTextAttribute(hConsole, saved_attributes);
            
            //cin >> serviceExecutablePath;
            getline(cin, serviceParameters);
            continue;

        }
        else {
            cout << "All values are already set." << flush;
        }
    } while (serviceDisplayName.empty() || serviceDescription.empty() || serviceParameters.empty());


    std::string escapedServiceParameters = serviceParameters;
    for (size_t quotePosition = 0; (quotePosition = escapedServiceParameters.find('"', quotePosition)) != std::string::npos; quotePosition += 2) {
        escapedServiceParameters.replace(quotePosition, 1, "\\\"");
    }

    finalServiceString = "sc create \"" + serviceDisplayName + "\" binPath= \"\\\"" + serviceExecutablePath + "\\\" " + escapedServiceParameters + "\"  DisplayName= \"" + serviceDisplayName + "\" start= auto";
    std::string deleteserviceString = "sc delete \"" + serviceDisplayName + "\"";

    //std::cout << "Hello World!\n" << flush;
    writeTextToFile(workingpath + "install_" + serviceDisplayName + ".bat", finalServiceString);
    writeTextToFile(workingpath + "uninstall_" + serviceDisplayName + ".bat", deleteserviceString);
    std:: cout << "Message written to " + workingpath + "*" + serviceDisplayName + ".bat" << std::endl;
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
