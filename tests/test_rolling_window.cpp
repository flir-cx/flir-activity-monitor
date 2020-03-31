#include "gtest/gtest.h"

#include "../rolling_window.hpp"

TEST(RollingWindow, AddValues) {
    const int window_size = 10;
    RollingWindow<int> window(window_size);

    EXPECT_EQ(window.isFullyPopulated(), false);

    for (int i = 0; i < window_size; ++i)
        window.addValue(10);

    EXPECT_EQ(window.isFullyPopulated(), true);
}

TEST(RollingWindow, TestLimit) {
    const int window_size = 10;
    RollingWindow<int> window(window_size);

    for (int i = 0; i < window_size; ++i)
        window.addValue(10);

    EXPECT_EQ(window.isFullyPopulated(), true);
    EXPECT_EQ(window.allValuesAreBelow(5), false);

    for (int i = 0; i < window_size - 1; ++i)
        window.addValue(4);

    EXPECT_EQ(window.allValuesAreBelow(5), false);
    window.addValue(4);

    EXPECT_EQ(window.allValuesAreBelow(5), true);

    window.addValue(5);
    EXPECT_EQ(window.allValuesAreBelow(5), false);
}


TEST(RollingWindow, TestReset) {

    const int window_size = 10;
    RollingWindow<int> window(window_size);

    for (int i = 0; i < window_size; ++i)
        window.addValue(10);

    EXPECT_EQ(window.isFullyPopulated(), true);

    window.reset();

    EXPECT_EQ(window.isFullyPopulated(), false);

    for (int i = 0; i < window_size - 1; ++i)
        window.addValue(10);

    EXPECT_EQ(window.isFullyPopulated(), false);

    window.addValue(10);

    EXPECT_EQ(window.isFullyPopulated(), true);
}
