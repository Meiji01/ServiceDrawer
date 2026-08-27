// createservice.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
using namespace std;

#include "writer.h"
#include <windows.h>
#include "crypto.h"

int main()
{

    //get the current working directory
    char buffer[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, buffer);
    std::string workingpath(buffer);
    workingpath += "\\"; // Add a backslash to the end of the path


    //string variables
    //std::string serviceName;
    std::string serviceDisplayName;
    std::string serviceDescription;
    std::string serviceExecutablePath;
    std::string finalServiceString;
    std::string sha256hex;

    //need to input the text to write to the file and the file name
    std::string currentvalue = "";

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
        else if (serviceExecutablePath.empty()) {
            cout << "Enter service executable path + params: " << flush;
            //cin >> serviceExecutablePath;
            getline(cin, serviceExecutablePath);
            continue;
        }
        else {
            cout << "All values are already set." << flush;
        }
    } while (serviceDisplayName.empty() || serviceDescription.empty() || serviceExecutablePath.empty());

    //get sha256 hex of the file
    getSHA256(serviceExecutablePath, sha256hex);
    cout << "SHA256 of " << serviceExecutablePath << ": " << sha256hex << std::endl << flush;

    finalServiceString = "sc create \"" + serviceDisplayName + "\" binPath= \"" + serviceExecutablePath + "\"  DisplayName= \"" + serviceDisplayName + "\" start= auto";

    std::cout << "Hello World!\n" << flush;
    writeTextToFile(workingpath + "output.txt", finalServiceString);
    std:: cout << "Message written to " + workingpath + "output.txt" << std::endl;
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
