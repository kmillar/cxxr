/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 2014 and onwards the Rho Project Authors.
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
#include "rho/RValue.hpp"

namespace rho {

TEST(RValueTest, scalarInteger) {
  auto value = RValue<IntVector>::scalar(54);
  EXPECT_EQ(INTSXP, value->sexptype());
  EXPECT_EQ(1, value->size());
  EXPECT_EQ(54, INTEGER(value)[0]);

  EXPECT_FALSE(Rf_isNull(value));
  EXPECT_FALSE(Rf_isObject(value));
  EXPECT_FALSE(IS_S4_OBJECT(value));
  EXPECT_TRUE(ATTRIB(value) == nullptr);

  EXPECT_EQ(54, Rf_asInteger(value));
  EXPECT_EQ(1, Rf_asLogical(value));
  EXPECT_EQ(54., Rf_asReal(value));

  INTEGER(value)[0] = -55;
  EXPECT_EQ(INTSXP, value->sexptype());
  EXPECT_EQ(1, value->size());
  EXPECT_EQ(-55, INTEGER(value)[0]);


}


}  // namespace rho
