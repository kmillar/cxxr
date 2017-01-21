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
#include "rho/IntVector.hpp"
#include "rho/Promise.hpp"

using namespace rho;

namespace {

class TestValgen : public RObject {
  public:
    TestValgen(int* evaluation_count, RObject* return_value)
        : m_evaluation_count(evaluation_count), m_return_value(return_value) {}

    RObject* evaluate(Environment* env) override {
        ++*m_evaluation_count;
        return m_return_value;
    }
  private:
    int* m_evaluation_count;
    RObject* m_return_value;
};

}  // anonymous namespace

TEST(PromiseTest, NoDoubleEvalAfterCopyingPromiseData) {
    // We need an RObject that we can track evaluation for.
    int num_evaluations = 0;
    RObject* return_value = IntVector::create(2);
    RObject* valgen = new TestValgen(&num_evaluations, return_value);

    PromiseData promise_data = PromiseData(valgen, nullptr);
    EXPECT_EQ(return_value, promise_data.evaluate());
    EXPECT_EQ(1, num_evaluations);
    // Chcek that evaluating the promise doesn't re-evaluate the expression.
    EXPECT_EQ(return_value, promise_data.evaluate());
    EXPECT_EQ(1, num_evaluations);

    num_evaluations = 0;
    promise_data = PromiseData(valgen, nullptr);
    PromiseData tmp = promise_data;
    EXPECT_EQ(0, num_evaluations);
    EXPECT_EQ(return_value, promise_data.evaluate());
    EXPECT_EQ(1, num_evaluations);
    EXPECT_EQ(return_value, tmp.evaluate());
    EXPECT_EQ(1, num_evaluations);
}

TEST(PromiseTest, NoDoubleEvalAfterAsPromise) {
    // We need an RObject that we can track evaluation for.
    int num_evaluations = 0;
    RObject* return_value = IntVector::create(2);
    RObject* valgen = new TestValgen(&num_evaluations, return_value);

    PromiseData promise_data = PromiseData(valgen, nullptr);
    Promise* promise = promise_data.asPromise();
    EXPECT_EQ(0, num_evaluations);
    EXPECT_EQ(return_value, promise->force());
    EXPECT_EQ(1, num_evaluations);
    EXPECT_EQ(return_value, promise_data.evaluate());
    EXPECT_EQ(1, num_evaluations);
}
