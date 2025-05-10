#pragma once

struct employee {
    int num;
    char name[10];
    double hours;
};

constexpr char PIPE_NAME[] = "\\\\.\\pipe\\EmployeeDataPipe";
constexpr int BUFFER_SIZE = 1024;

enum class Command {
    READ,
    WRITE,
    RELEASE_READ,
    RELEASE_WRITE,
    EXIT
};

constexpr char CLIENT_EXE[] = "Client.exe";