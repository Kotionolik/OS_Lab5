#include <iostream>
#include <limits>
#include "employee.h"
#include <boost/asio.hpp>

namespace asio = boost::asio;
using asio::ip::tcp;

void HandleRead(tcp::socket& socket) {
    int id;
    std::cout << "Enter employee ID to read: ";
    std::cin >> id;

    Command cmd = Command::READ;
    asio::write(socket, asio::buffer(&cmd, sizeof(cmd)));
    asio::write(socket, asio::buffer(&id, sizeof(id)));

    employee emp;
    asio::read(socket, asio::buffer(&emp, sizeof(emp)));

    if (emp.num == -1) {
        std::cout << "Employee not found!\n";
        return;
    }

    std::cout << "Employee Data:\n"
        << "  ID: " << emp.num << "\n"
        << "  Name: " << emp.name << "\n"
        << "  Hours: " << emp.hours << "\n";
}

void HandleWrite(tcp::socket& socket) {
    int id;
    std::cout << "Enter employee ID to modify: ";
    std::cin >> id;

    Command cmd = Command::WRITE;
    asio::write(socket, asio::buffer(&cmd, sizeof(cmd)));
    asio::write(socket, asio::buffer(&id, sizeof(id)));

    employee emp;
    asio::read(socket, asio::buffer(&emp, sizeof(emp)));

    if (emp.num == -1) {
        std::cout << "Employee not found!\n";
        return;
    }

    std::cout << "Current Data:\n"
        << "  ID: " << emp.num << "\n"
        << "  Name: " << emp.name << "\n"
        << "  Hours: " << emp.hours << "\n";

    std::cin.ignore();
    std::cout << "Enter new name: ";
    std::cin.getline(emp.name, sizeof(emp.name));
    if (std::cin.fail()) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Warning: Entered name is greater than 9 symbols and therefore was cut.\n";
    }
    std::cout << "Enter new hours: ";
    std::cin >> emp.hours;
    while (emp.hours < 0) {
        std::cout << "Error: Employee hours value should be at least zero. Enter again: ";
        std::cin >> emp.hours;
    }

    asio::write(socket, asio::buffer(&emp, sizeof(emp)));
}

int main() {
    try {
        asio::io_context io_context;
        tcp::socket socket(io_context);
        tcp::resolver resolver(io_context);
        asio::connect(socket, resolver.resolve(SERVER_IP, std::to_string(SERVER_PORT)));

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
                HandleRead(socket);
                break;
            case 2:
                HandleWrite(socket);
                break;
            case 3: {
                Command cmd = Command::EXIT;
                asio::write(socket, asio::buffer(&cmd, sizeof(cmd)));
                return 0;
            }
            default:
                std::cout << "Invalid option!\n";
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}