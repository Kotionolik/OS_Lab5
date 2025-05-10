#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>
#include <string>
#include "employee.h"
#define NOMINMAX
#include <Windows.h>
#undef max

void CreateDataFile(const std::string& filename);
void PrintDataFile();
bool FindEmployeeInFile(int id);
bool FindEmployeeInFile(int id, employee& result);
bool UpdateEmployeeInFile(const employee& emp);
DWORD WINAPI ProcessClient(LPVOID lpParam);
void LaunchClientProcesses(int count);

std::string filename;
HANDLE hFile = INVALID_HANDLE_VALUE;
std::vector<employee> employees;
std::map<int, HANDLE> recordMutexes;
CRITICAL_SECTION globalCS;
bool serverRunning = true;
std::vector<HANDLE> clientProcesses;
std::vector<HANDLE> threadHandles;
HANDLE hFileMutex = NULL;

void CreateDataFile(const std::string& filename) {
    hFile = CreateFileA(
        filename.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to create file: " << filename << ". GLE=" << GetLastError() << std::endl;
        exit(1);
    }

    hFileMutex = CreateMutexA(NULL, FALSE, "Global\\EmployeeFileMutex");
    if (hFileMutex == NULL) {
        std::cerr << "CreateMutex failed. GLE=" << GetLastError() << std::endl;
    }

    std::cout << "Enter number of employees: ";
    int count;
    std::cin >> count;

    for (int i = 0; i < count; ++i) {
        employee emp;
        std::cout << "Employee " << i + 1 << ":\n";
        std::cout << "  ID: "; std::cin >> emp.num;
        while (emp.num < 1)
        {
            std::cout << "Error: ID value should be at least one. Enter again: "; std::cin >> emp.num;
        }
        std::cin.getline(emp.name, sizeof(emp.name));
        std::cout << "  Name: "; std::cin.getline(emp.name, sizeof(emp.name));
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Warning: Entered name is greater than 9 symbols and therefore was cut.\n";
        }
        std::cout << "  Hours: "; std::cin >> emp.hours;
        while (emp.hours < 0)
        {
            std::cout << "Error: Employee hours value should be at least zero. Enter again: "; std::cin >> emp.hours;
        }
        while (FindEmployeeInFile(emp.num))
        {
            std::cout << "Error: such an ID already exists. Enter again: "; std::cin >> emp.num;
        }
        DWORD bytesWritten;
        WaitForSingleObject(hFileMutex, INFINITE);
        WriteFile(hFile, &emp, sizeof(employee), &bytesWritten, NULL);
        ReleaseMutex(hFileMutex);

        wchar_t mutexName[50];
        swprintf_s(mutexName, 50, L"Global\\EmployeeMutex_%d", emp.num);
        recordMutexes[emp.num] = CreateMutexW(NULL, FALSE, mutexName);
        if (recordMutexes[emp.num] == NULL) {
            std::cerr << "CreateMutex failed. GLE=" << GetLastError() << std::endl;
        }
    }
}

void PrintDataFile() {

    WaitForSingleObject(hFileMutex, INFINITE);
    if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        std::cerr << "SetFilePointer failed. GLE=" << GetLastError() << std::endl;
        ReleaseMutex(hFileMutex);
        return;
    }

    std::cout << "\nFile Content:\n";
    employee emp;
    DWORD bytesRead;

    while (ReadFile(hFile, &emp, sizeof(employee), &bytesRead, NULL) && bytesRead == sizeof(employee)) {
        std::cout << "ID: " << emp.num
            << ", Name: " << emp.name
            << ", Hours: " << emp.hours << "\n";
    }
    std::cout << std::endl;
    ReleaseMutex(hFileMutex);
}

bool FindEmployeeInFile(int id) {
    WaitForSingleObject(hFileMutex, INFINITE);
    if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        ReleaseMutex(hFileMutex);
        return false;
    }

    employee emp;
    DWORD bytesRead;
    bool found = false;

    while (ReadFile(hFile, &emp, sizeof(employee), &bytesRead, NULL)) {
        if (bytesRead != sizeof(employee)) break;
        if (emp.num == id) {
            found = true;
        }
    }

    ReleaseMutex(hFileMutex);
    return found;
}

bool FindEmployeeInFile(int id, employee& result) {
    WaitForSingleObject(hFileMutex, INFINITE);
    if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        ReleaseMutex(hFileMutex);
        return false;
    }

    employee emp;
    DWORD bytesRead;
    bool found = false;

    while (ReadFile(hFile, &emp, sizeof(employee), &bytesRead, NULL)) {
        if (bytesRead != sizeof(employee)) break;
        if (emp.num == id) {
            result = emp;
            found = true;
            break;
        }
    }

    ReleaseMutex(hFileMutex);
    return found;
}

bool UpdateEmployeeInFile(const employee& emp) {
    WaitForSingleObject(hFileMutex, INFINITE);
    if (SetFilePointer(hFile, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
        ReleaseMutex(hFileMutex);
        return false;
    }

    employee current;
    DWORD bytesRead;
    LARGE_INTEGER position = { 0 };
    bool found = true;

    while (ReadFile(hFile, &current, sizeof(employee), &bytesRead, NULL) && bytesRead == sizeof(employee)) {
        if (current.num == emp.num) {
            LARGE_INTEGER newPos;
            newPos.QuadPart = position.QuadPart;

            if (SetFilePointerEx(hFile, newPos, NULL, FILE_BEGIN)) {
                DWORD bytesWritten;
                if (WriteFile(hFile, &emp, sizeof(employee), &bytesWritten, NULL) &&
                    bytesWritten == sizeof(employee)) {
                    found = true;
                }
            }
            break;
        }
        position.QuadPart += sizeof(employee);
    }
    ReleaseMutex(hFileMutex);
    return found;

}

DWORD WINAPI ProcessClient(LPVOID lpParam) {
    HANDLE pipe = static_cast<HANDLE>(lpParam);
    while (serverRunning) {
        Command cmd;
        DWORD bytesRead;
        if (!ReadFile(pipe, &cmd, sizeof(cmd), &bytesRead, NULL)) {
            DWORD err = GetLastError();
            if (err != ERROR_BROKEN_PIPE && err != ERROR_NO_DATA) {
                std::cerr << "ReadFile failed. GLE=" << err << std::endl;
            }
            break;
        }

        if (bytesRead != sizeof(cmd)) {
            continue;
        }

        int id;
        if (!ReadFile(pipe, &id, sizeof(id), &bytesRead, NULL) || bytesRead != sizeof(id)) {
            break;
        }

        if (recordMutexes.find(id) == recordMutexes.end()) {
            employee invalidEmp{ -1, "", 0.0 };
            WriteFile(pipe, &invalidEmp, sizeof(invalidEmp), NULL, NULL);

            Command releaseCmd;
            ReadFile(pipe, &releaseCmd, sizeof(releaseCmd), &bytesRead, NULL);
            continue;
        }

        HANDLE hMutex = recordMutexes[id];
        employee emp;
        bool found = false;

        switch (cmd) {
        case Command::READ: {
            WaitForSingleObject(hMutex, INFINITE);
            found = FindEmployeeInFile(id, emp);
            ReleaseMutex(hMutex);

            if (!found) {
                emp.num = -1;
            }

            WriteFile(pipe, &emp, sizeof(emp), NULL, NULL);

            Command releaseCmd;
            ReadFile(pipe, &releaseCmd, sizeof(releaseCmd), &bytesRead, NULL);
            break;
        }
        case Command::WRITE: {
            WaitForSingleObject(hMutex, INFINITE);

            found = FindEmployeeInFile(id, emp);

            if (!found) {
                emp.num = -1;
            }

            WriteFile(pipe, &emp, sizeof(emp), NULL, NULL);

            if (ReadFile(pipe, &emp, sizeof(emp), &bytesRead, NULL) && bytesRead == sizeof(emp)) {
                if (emp.num != -1) {
                    UpdateEmployeeInFile(emp);
                }
            }

            Command releaseCmd;
            ReadFile(pipe, &releaseCmd, sizeof(releaseCmd), &bytesRead, NULL);

            ReleaseMutex(hMutex);
            break;
        }
        case Command::EXIT:
            break;
        default:
            break;
        }
    }

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

void LaunchClientProcesses(int count) {
    for (int i = 0; i < count; ++i) {
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcessA(
            CLIENT_EXE,
            NULL,
            NULL,
            NULL,
            FALSE,
            CREATE_NEW_CONSOLE,
            NULL,
            NULL,
            &si,
            &pi
        )) {
            std::cerr << "CreateProcess failed. GLE=" << GetLastError() << std::endl;
        }
        else {
            clientProcesses.push_back(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
}

#ifndef TESTING
int main() {
    InitializeCriticalSection(&globalCS);
    std::cout << "Enter filename: ";
    std::cin >> filename;
    CreateDataFile(filename);
    PrintDataFile();

    int clientCount = 0;
    std::cout << "Enter number of clients: ";
    std::cin >> clientCount;
    while (clientCount < 1)
    {
        std::cout << "Error: number of clients is less than one. Enter again: ";
        std::cin >> clientCount;
    }

    LaunchClientProcesses(clientCount);

    for (int i = 0; i < clientCount; ++i) {
        HANDLE hPipe = CreateNamedPipeA(
            PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            BUFFER_SIZE,
            BUFFER_SIZE,
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            std::cerr << "Named pipe creation failed. GLE=" << GetLastError() << std::endl;
            continue;
        }

        if (!ConnectNamedPipe(hPipe, NULL)) {
            DWORD err = GetLastError();
            if (err != ERROR_PIPE_CONNECTED) {
                std::cerr << "ConnectNamedPipe failed. GLE=" << err << std::endl;
                CloseHandle(hPipe);
                continue;
            }
        }

        HANDLE hThread = CreateThread(
            NULL,
            0,
            ProcessClient,
            hPipe,
            0,
            NULL
        );

        if (hThread == NULL) {
            std::cerr << "Thread creation failed. GLE=" << GetLastError() << std::endl;
            CloseHandle(hPipe);
        }
        else {
            threadHandles.push_back(hThread);
        }
    }

    WaitForMultipleObjects(
        static_cast<DWORD>(clientProcesses.size()),
        clientProcesses.data(),
        TRUE,
        INFINITE
    );

    for (HANDLE hProcess : clientProcesses) {
        CloseHandle(hProcess);
    }

    serverRunning = false;

    for (HANDLE hThread : threadHandles) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }

    PrintDataFile();

    system("pause");

    for (auto& mutex : recordMutexes) {
        if (mutex.second) CloseHandle(mutex.second);
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }

    DeleteCriticalSection(&globalCS);

    return 0;
}
#endif