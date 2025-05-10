#include <gtest/gtest.h>
#include <Windows.h>
#include <fstream>
#include <string>
#include "employee.h"

#define TESTING
#include "Server.cpp"

class ServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Сохраняем оригинальные глобальные переменные
        oldHFile = hFile;
        oldHFileMutex = hFileMutex;

        // Создаем временный файл
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        sprintf_s(tempFileName, "%stempfile_XXXXXX", tempPath);
        filename = mktemp(tempFileName);

        hFileMutex = CreateMutexA(NULL, FALSE, NULL);
        hFile = CreateFileA(
            filename.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        // Записываем тестовые данные
        employee emp1 = { 1, "Alice", 40.0 };
        employee emp2 = { 2, "Bob", 30.5 };
        DWORD bytesWritten;
        WriteFile(hFile, &emp1, sizeof(employee), &bytesWritten, NULL);
        WriteFile(hFile, &emp2, sizeof(employee), &bytesWritten, NULL);
        FlushFileBuffers(hFile);
    }

    void TearDown() override {
        // Восстанавливаем глобальные переменные
        hFile = oldHFile;
        hFileMutex = oldHFileMutex;

        // Закрываем временные ресурсы
        if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
        if (hFileMutex != NULL) CloseHandle(hFileMutex);
        DeleteFileA(filename.c_str());
    }

    HANDLE oldHFile;
    HANDLE oldHFileMutex;
    char tempFileName[MAX_PATH];
    std::string filename;
};

TEST_F(ServerTest, FindExistingEmployee) {
    employee result;
    bool found = FindEmployeeInFile(1, result);
    EXPECT_TRUE(found);
    EXPECT_EQ(result.num, 1);
    EXPECT_STREQ(result.name, "Alice");
    EXPECT_DOUBLE_EQ(result.hours, 40.0);
}

TEST_F(ServerTest, FindNonExistingEmployee) {
    employee result;
    bool found = FindEmployeeInFile(3, result);
    EXPECT_FALSE(found);
}

TEST_F(ServerTest, UpdateEmployee) {
    employee updated = { 1, "Alicia", 45.0 };
    bool success = UpdateEmployeeInFile(updated);
    EXPECT_TRUE(success);

    employee result;
    FindEmployeeInFile(1, result);
    EXPECT_STREQ(result.name, "Alicia");
    EXPECT_DOUBLE_EQ(result.hours, 45.0);
}