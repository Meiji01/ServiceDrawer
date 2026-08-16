#pragma once

#include <string>

// Appends message to logs.txt, rotating (delete + recreate) once the file hits the 10MB size limit
void writeConsoleOutputToLog(const std::string& message);
