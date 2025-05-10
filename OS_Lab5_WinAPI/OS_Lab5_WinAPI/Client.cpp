#include <iostream>
#define NOMINMAX
#include <Windows.h>
#undef max
#include <limits>
#include "employee.h"

void HandleRead(HANDLE pipe) {
    int id;
    std::cout << "Enter employee ID to read: ";
    std::cin >> id;

    Command cmd = Command::READ;
    WriteFile(pipe, &cmd, sizeof(cmd), NULL, NULL);
    WriteFile(pipe, &id, sizeof(id), NULL, NULL);

    employee emp;
    DWORD bytesRead;
    ReadFile(pipe, &emp, sizeof(emp), &bytesRead, NULL);

    if (emp.num == -1) {
        std::cout << "Employee not found!\n";
        Command release = Command::RELEASE_READ;
        WriteFile(pipe, &release, sizeof(release), NULL, NULL);
        return;
    }

    std::cout << "Employee Data:\n"
        << "  ID: " << emp.num << "\n"
        << "  Name: " << emp.name << "\n"
        << "  Hours: " << emp.hours << "\n";

    Command release = Command::RELEASE_READ;
    WriteFile(pipe, &release, sizeof(release), NULL, NULL);
}

void HandleWrite(HANDLE pipe) {
    int id;
    std::cout << "Enter employee ID to modify: ";
    std::cin >> id;

    Command cmd = Command::WRITE;
    WriteFile(pipe, &cmd, sizeof(cmd), NULL, NULL);
    WriteFile(pipe, &id, sizeof(id), NULL, NULL);

    employee emp;
    DWORD bytesRead;
    ReadFile(pipe, &emp, sizeof(emp), &bytesRead, NULL);

    if (emp.num == -1) {
        std::cout << "Employee not found!\n";
        Command release = Command::RELEASE_WRITE;
        WriteFile(pipe, &release, sizeof(release), NULL, NULL);
        return;
    }

    std::cout << "Current Data:\n"
        << "  ID: " << emp.num << "\n"
        << "  Name: " << emp.name << "\n"
        << "  Hours: " << emp.hours << "\n";

    std::cout << "Enter new name: ";
    std::cin.getline(emp.name, sizeof(emp.name));
    std::cin.getline(emp.name, sizeof(emp.name));
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Warning: Entered name is greater than 9 symbols and therefore was cut.\n";
    }
    std::cout << "Enter new hours: ";
    std::cin >> emp.hours;
    while (emp.hours < 0)
    {
        std::cout << "Error: Employee hours value should be at least zero. Enter again: "; std::cin >> emp.hours;
    }

    WriteFile(pipe, &emp, sizeof(emp), NULL, NULL);

    Command release = Command::RELEASE_WRITE;
    WriteFile(pipe, &release, sizeof(release), NULL, NULL);
}

int main() {
    HANDLE pipe = CreateFileA(
        PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (pipe == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to connect to pipe. GLE=" << GetLastError() << std::endl;
        return 1;
    }

    while (true) {
        std::cout << "\nOptions:\n"
            << "1. Read employee\n"
            << "2. Modify employee\n"
            << "3. Exit\n"
            << "Choice: ";

        int choice;
        std::cin >> choice;

        switch (choice) {
        case 1:
            HandleRead(pipe);
            break;
        case 2:
            HandleWrite(pipe);
            break;
        case 3: {
            Command cmd = Command::EXIT;
            WriteFile(pipe, &cmd, sizeof(cmd), NULL, NULL);
            CloseHandle(pipe);
            return 0;
        }
        default:
            std::cout << "Invalid option!\n";
        }
    }
}