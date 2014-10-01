#include <fstream>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <stdio.h>

#include "testlog.h"

std::string TLoggedFixture::BaseDir = ".";

TLoggedFixture::~TLoggedFixture() {}

void TLoggedFixture::TearDown()
{
    std::string file_name = GetFileName(".out");
    if (IsOk()) {
        if (remove(file_name.c_str()) < 0) {
            if (errno != ENOENT)
                FAIL() << "failed to remove .out file: " << file_name;
        }
        return;
    }
    std::ofstream f(file_name);
    if (!f.is_open())
        FAIL() << "cannot create file for failing logged test: " << file_name;
    else {
        f << Contents.str();
        ADD_FAILURE() << "test diff: " << GetFileName();
    }
}

bool TLoggedFixture::IsOk()
{
    std::ifstream f(GetFileName());
    if (!f)
        return !Contents.tellp();

    std::stringstream buf;
    buf << f.rdbuf();
    return buf.str() == Contents.str();
}

std::string TLoggedFixture::GetFileName(const std::string& suffix) const
{
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    std::string result = BaseDir + "/" +
        test_info->test_case_name() + "." + test_info->name() +
        ".dat" + suffix;
    return result;
}

void TLoggedFixture::SetExecutableName(const std::string& file_name)
{
    char* s = strdup(file_name.c_str());
    BaseDir = dirname(s);
    free(s);
}