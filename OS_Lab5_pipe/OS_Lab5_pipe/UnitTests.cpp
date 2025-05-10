#include <gtest/gtest.h>
#include <fstream>
#include <mutex>
#include "employee.h"

extern void CreateDataFile(const std::string& filename);
extern bool FindEmployeeInFile(int id, employee& result);
extern bool UpdateEmployeeInFile(const employee& emp);
extern std::fstream dataFile;
extern std::mutex fileMutex;

class ServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        filename = "test_data.bin";
        CreateDataFile(filename);

        employee emp1{ 1, "Alice", 40.0 };
        employee emp2{ 2, "Bob", 30.5 };

        {
            std::lock_guard<std::mutex> lock(fileMutex);
            dataFile.open(filename, std::ios::out | std::ios::binary | std::ios::trunc);
            dataFile.write(reinterpret_cast<const char*>(&emp1), sizeof(employee));
            dataFile.write(reinterpret_cast<const char*>(&emp2), sizeof(employee));
            dataFile.close();
            dataFile.open(filename, std::ios::in | std::ios::out | std::ios::binary);
        }
    }

    void TearDown() override {
        dataFile.close();
        std::remove(filename.c_str());
    }

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