#pragma once

enum class Command { READ, WRITE, EXIT, RELEASE_READ, RELEASE_WRITE };

constexpr int BUFFER_SIZE = 1024;
constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 12345;
constexpr const char* CLIENT_EXE = "Client.exe";

struct employee {
    int num;
    char name[10];
    double hours;
};