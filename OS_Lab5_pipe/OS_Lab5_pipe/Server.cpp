#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <string>
#include <cstring>
#include <algorithm>
#include <limits>
#include "employee.h"
#include <boost/asio.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

std::string filename;
std::fstream dataFile;
std::map<int, std::mutex> recordMutexes;
std::mutex fileMutex;

void CreateDataFile(const std::string& filename) {
    dataFile.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!dataFile.is_open()) {
        std::cerr << "Failed to create file: " << filename << std::endl;
        exit(1);
    }

    std::cout << "Enter number of employees: ";
    int count;
    std::cin >> count;

    for (int i = 0; i < count; ++i) {
        employee emp;
        std::cout << "Employee " << i + 1 << ":\n";
        std::cout << "  ID: "; std::cin >> emp.num;
        while (emp.num < 1) {
            std::cout << "Error: ID value should be at least one. Enter again: ";
            std::cin >> emp.num;
        }
        std::cin.ignore();
        std::cout << "  Name: ";
        std::cin.getline(emp.name, sizeof(emp.name));
        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Warning: Entered name is greater than 9 symbols and therefore was cut.\n";
        }
        std::cout << "  Hours: "; std::cin >> emp.hours;
        while (emp.hours < 0) {
            std::cout << "Error: Employee hours value should be at least zero. Enter again: ";
            std::cin >> emp.hours;
        }

        dataFile.write(reinterpret_cast<const char*>(&emp), sizeof(employee));
        recordMutexes[emp.num];
    }
    dataFile.close();
    dataFile.open(filename, std::ios::in | std::ios::out | std::ios::binary);
}

void PrintDataFile() {
    std::lock_guard<std::mutex> lock(fileMutex);
    dataFile.seekg(0, std::ios::beg);
    std::cout << "\nFile Content:\n";
    employee emp;

    while (dataFile.read(reinterpret_cast<char*>(&emp), sizeof(employee))) {
        std::cout << "ID: " << emp.num
            << ", Name: " << emp.name
            << ", Hours: " << emp.hours << "\n";
    }
    std::cout << std::endl;
    dataFile.clear();
}

bool FindEmployeeInFile(int id, employee& result) {
    std::lock_guard<std::mutex> lock(fileMutex);
    dataFile.seekg(0, std::ios::beg);
    employee emp;

    while (dataFile.read(reinterpret_cast<char*>(&emp), sizeof(employee))) {
        if (emp.num == id) {
            result = emp;
            return true;
        }
    }
    return false;
}

bool UpdateEmployeeInFile(const employee& emp) {
    std::lock_guard<std::mutex> lock(fileMutex);
    dataFile.seekg(0, std::ios::beg);
    employee current;
    std::streampos pos = 0;

    while (dataFile.read(reinterpret_cast<char*>(&current), sizeof(employee))) {
        if (current.num == emp.num) {
            dataFile.seekp(pos);
            dataFile.write(reinterpret_cast<const char*>(&emp), sizeof(employee));
            dataFile.flush();
            return true;
        }
        pos = dataFile.tellg();
    }
    return false;
}

void HandleClient(tcp::socket socket) {
    try {
        while (true) {
            Command cmd;
            boost::system::error_code ec;
            asio::read(socket, asio::buffer(&cmd, sizeof(cmd)), ec);

            if (ec) break;

            int id;
            asio::read(socket, asio::buffer(&id, sizeof(id)), ec);
            if (ec) break;

            if (recordMutexes.find(id) == recordMutexes.end()) {
                employee invalidEmp{ -1, "", 0.0 };
                asio::write(socket, asio::buffer(&invalidEmp, sizeof(invalidEmp)));
                continue;
            }

            employee emp;
            bool found = false;

            switch (cmd) {
            case Command::READ: {
                std::lock_guard<std::mutex> lock(recordMutexes[id]);
                found = FindEmployeeInFile(id, emp);
                if (!found) emp.num = -1;
                asio::write(socket, asio::buffer(&emp, sizeof(emp)));
                break;
            }
            case Command::WRITE: {
                std::lock_guard<std::mutex> lock(recordMutexes[id]);
                found = FindEmployeeInFile(id, emp);
                if (!found) emp.num = -1;
                asio::write(socket, asio::buffer(&emp, sizeof(emp)));

                asio::read(socket, asio::buffer(&emp, sizeof(emp)), ec);
                if (!ec && emp.num != -1) {
                    UpdateEmployeeInFile(emp);
                }
                break;
            }
            case Command::EXIT:
                return;
            default:
                break;
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Client handling exception: " << e.what() << std::endl;
    }
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
            std::cerr << "CreateProcess failed. Error: " << GetLastError() << std::endl;
        }
        else {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
}

int main() {
    std::cout << "Enter filename: ";
    std::cin >> filename;
    CreateDataFile(filename);
    PrintDataFile();

    int clientCount = 0;
    std::cout << "Enter number of clients: ";
    std::cin >> clientCount;
    while (clientCount < 1) {
        std::cout << "Error: number of clients is less than one. Enter again: ";
        std::cin >> clientCount;
    }

    asio::io_context io_context;
    tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), SERVER_PORT));
    std::vector<std::thread> threads;

    LaunchClientProcesses(clientCount);

    for (int i = 0; i < clientCount; ++i) {
        tcp::socket socket(io_context);
        acceptor.accept(socket);
        threads.emplace_back([s = std::move(socket)]() mutable {
            HandleClient(std::move(s));
            });
    }

    for (auto& thread : threads) {
        if (thread.joinable()) thread.join();
    }

    PrintDataFile();
    dataFile.close();

    std::cout << "Press Enter to exit...";
    std::cin.ignore();
    std::cin.get();

    return 0;
}