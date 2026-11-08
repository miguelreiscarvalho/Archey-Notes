#include <gtest/gtest.h>
#include "gui/RadialMenuGeometry.h"
TEST(RadialMenuGeometry, CornersFitTheExpandedMenu) {
    for(double extent:{340.,600.,1200.})for(double x:{0.,34.,300.,1200.}) {
        double size=std::min(500.,extent-16.);
        double center=xoj::radial::clampCenter(x,extent,size);
        EXPECT_GE(center-size/2,8.);EXPECT_LE(center+size/2,extent-8.);
    }
}
TEST(RadialMenuGeometry, TapAndDragHaveDistinctThresholds) {
    EXPECT_FALSE(xoj::radial::isDrag(4,4));EXPECT_FALSE(xoj::radial::isDrag(7,0));
    EXPECT_TRUE(xoj::radial::isDrag(8,0));EXPECT_TRUE(xoj::radial::isDrag(6,6));
}
TEST(RadialMenuGeometry, AnimationHasStableEndpoints) {
    EXPECT_EQ(xoj::radial::smooth(0),0);EXPECT_EQ(xoj::radial::smooth(1),1);
    EXPECT_EQ(xoj::radial::smooth(-1),0);EXPECT_EQ(xoj::radial::smooth(2),1);
    double previous=0;for(int i=0;i<=100;++i) {double p=xoj::radial::smooth(i/100.);EXPECT_GE(p,previous);previous=p;}
}
TEST(RadialMenuGeometry, OptionsRemainAdjacentInShortFan) {
    for(double radius:{70.,120.,160.,175.})for(size_t count:{size_t(2),size_t(5),size_t(6)}) {
        double previous=xoj::radial::optionAngle(-1.5,0,count,radius);
        for(size_t i=1;i<count;++i) {
            double angle=xoj::radial::optionAngle(-1.5,i,count,radius);
            double distance=2*radius*std::sin((angle-previous)/2);
            EXPECT_NEAR(distance,54,1e-8); // 48 px button + 6 px gap
            previous=angle;
        }
    }
}
