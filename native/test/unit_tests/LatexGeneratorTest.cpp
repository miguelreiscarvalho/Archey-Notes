#include <gtest/gtest.h>
#include "control/latex/LatexGenerator.h"
#include "control/settings/LatexSettings.h"
#include "util/PathUtil.h"

TEST(LatexGenerator, RecompileWithWindowsPathsAndSpaces) {
    const char* compiler = g_getenv("XOURNAL_LATEX_TEST_COMPILER");
    if (!compiler) GTEST_SKIP() << "Set XOURNAL_LATEX_TEST_COMPILER for the real compiler integration test";
    auto folder = Util::getTmpDirSubfolder("latex path with spaces");
    fs::create_directories(folder);
    LatexSettings settings;
    for (const char* placeholder : {"{}", "'{}'"}) {
        settings.genCmd = std::string("'") + compiler + "' -no-shell-escape -halt-on-error -interaction=nonstopmode " + placeholder;
        LatexGenerator generator(settings);
        for (const char* formula : {"x=1", "x=", "x=1"}) {
            auto result = generator.asyncRun(folder, std::string("\\documentclass{article}\\begin{document}$") + formula + "$\\end{document}");
            auto** process = std::get_if<GSubprocess*>(&result);
            ASSERT_NE(process, nullptr);
            gchar* output = nullptr;
            GError* error = nullptr;
            ASSERT_TRUE(g_subprocess_communicate_utf8(*process, nullptr, nullptr, &output, nullptr, &error));
            EXPECT_EQ(g_subprocess_get_exit_status(*process), 0) << (output ? output : "");
            g_free(output);
            g_clear_error(&error);
            g_object_unref(*process);
        }
    }
    fs::remove_all(folder);
}
