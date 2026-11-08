#include <gtest/gtest.h>
#include "control/TracosLayout.h"

TEST(TracosLayout, UnevenWordsBecomeReadingOrderParagraph) {
    EXPECT_EQ(tracos::paragraph({{"mundo",{80,24,120,40}}, {"Ola",{20,20,50,36}},
                                {"continua",{20,65,70,81}}, {"aqui",{130,18,160,34}}}),
              "Ola mundo aqui\ncontinua");
}
TEST(TracosLayout, SourceRowsKeepTheirVerticalPositions) {
    auto rows=tracos::composeLines({{"casa",{20,30,80,50}}, {"nova",{20,95,75,115}},
                                  {",",{85,49,88,56}}, {".",{80,114,81,115}}});
    ASSERT_EQ(rows.size(),2);
    EXPECT_EQ(rows[0].text(),"casa,");EXPECT_EQ(rows[1].text(),"nova.");
    EXPECT_DOUBLE_EQ(rows[0].source.top,30);EXPECT_DOUBLE_EQ(rows[1].source.top,95);
}
TEST(TracosLayout, LateAccentDoesNotChangeWordOrder) {
    EXPECT_EQ(tracos::paragraph({{"mundo",{80,24,120,40}}, {"Ol\xc3\xa1",{20,10,50,36}}}),
              "Ol\xc3\xa1 mundo");
}
TEST(TracosLayout, PunctuationHasNoLeadingSpace) {
    EXPECT_EQ(tracos::paragraph({{"Ola",{20,20,50,36}}, {",",{55,30,58,36}},
                                {"mundo",{80,24,120,40}}}), "Ola, mundo");
}
TEST(TracosLayout, SeparateRegionAndFollowingLine) {
    tracos::Box box{20,20,140,40};
    EXPECT_TRUE(box.near({20,60,80,80}));
    EXPECT_TRUE(box.near({30,10,40,15}));
    EXPECT_FALSE(box.near({400,400,420,430}));
    EXPECT_TRUE(tracos::paragraph({}).empty());
    EXPECT_TRUE(box.startsBelow({20,62,50,80},54,12));
    EXPECT_FALSE(box.startsBelow({80,25,100,42},54,12));
    EXPECT_FALSE(box.startsBelow({145,58,146,59},54,12)); // punctuation below baseline
}
