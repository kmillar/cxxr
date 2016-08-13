/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2016 and onwards the Rho Project Authors.
 *
 *  Rho is not part of the R project, and bugs and other issues should
 *  not be reported via r-bugs or other R project channels; instead refer
 *  to the Rho website.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  http://www.r-project.org/Licenses/
 */

#include "gtest/gtest.h"
#include "rho/DiscriminatedUnion.hpp"
#include <cmath>

namespace rho {

TEST(DiscriminatedUnionTest, integer) {
    DiscriminatedUnion value;
    uint16_t tag = 35;
    value.setInteger(tag, 78);
    EXPECT_TRUE(value.isInteger(tag));
    EXPECT_FALSE(value.isInteger(tag + 1));
    EXPECT_FALSE(value.isInteger(0));
    EXPECT_EQ(78, value.getInteger(tag));
}

TEST(DiscriminatedUnionTest, double) {
    DiscriminatedUnion value;
    ASSERT_TRUE(DiscriminatedUnion::isStorableDoubleValue(2.3));
    value.setDouble(2.3);
    EXPECT_TRUE(value.isDouble());
    EXPECT_EQ(2.3, value.getDouble());
}

TEST(DiscriminatedUnionTest, double_zero) {
    DiscriminatedUnion value;
    ASSERT_TRUE(DiscriminatedUnion::isStorableDoubleValue(0));
    value.setDouble(0);
    EXPECT_TRUE(value.isDouble());
    EXPECT_FALSE(value.isEitherPointer());
    EXPECT_EQ(0, value.getDouble());
}

TEST(DiscriminatedUnionTest, double_inf) {
    double Inf = std::numeric_limits<double>::infinity();
    ASSERT_TRUE(DiscriminatedUnion::isStorableDoubleValue(Inf));
    ASSERT_TRUE(DiscriminatedUnion::isStorableDoubleValue(-Inf));
    DiscriminatedUnion value;
    ASSERT_TRUE(value.isStorableDoubleValue(Inf));
    value.setDouble(Inf);
    EXPECT_TRUE(value.isDouble());
    EXPECT_FALSE(value.isEitherPointer());
    EXPECT_TRUE(isinf(value.getDouble()));
}

TEST(DiscriminatedUnionTest, double_nan) {
    double nan = std::numeric_limits<double>::quiet_NaN();
    ASSERT_TRUE(DiscriminatedUnion::isStorableDoubleValue(nan));
    DiscriminatedUnion value;
    value.setDouble(nan);
    EXPECT_TRUE(value.isDouble());
    EXPECT_FALSE(value.isEitherPointer());
    EXPECT_TRUE(isnan(value.getDouble()));
}

TEST(DiscriminatedUnionTest, non_storable) {
    typedef std::numeric_limits<double> limits;

    EXPECT_FALSE(DiscriminatedUnion::isStorableDoubleValue(limits::min()));
    EXPECT_FALSE(DiscriminatedUnion::isStorableDoubleValue(-limits::min()));
    EXPECT_TRUE(DiscriminatedUnion::isStorableDoubleValue(
                    std::sqrt(limits::min())));
    EXPECT_TRUE(DiscriminatedUnion::isStorableDoubleValue(
                    -std::sqrt(limits::min())));
    EXPECT_TRUE(DiscriminatedUnion::isStorableDoubleValue(limits::max()));
    EXPECT_TRUE(DiscriminatedUnion::isStorableDoubleValue(-limits::max()));
}

}  // namespace rho
