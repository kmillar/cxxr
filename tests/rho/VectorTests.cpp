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
#include "rapidcheck/gtest.h"
#include "rapidcheck/state.h"
#include <vector>
#include "rho/Vector.hpp"

using namespace rho;

namespace {
template<typename T>
using VectorModel = std::vector<T>;

template<typename T>
using VectorCommand = rc::state::Command<VectorModel<T>, Vector<T>>;

// The ordering heree follows the ordering in Vector.hpp
template<typename T>
struct size : public VectorCommand<T> {
    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        RC_ASSERT(model.size() == vector.size());
    }
    void show(std::ostream& os) const override { os << "size()"; }
};

template<typename T>
struct resize : public VectorCommand<T> {
    int value = *rc::gen::inRange(0, 10);

    void checkPreconditions(const VectorModel<T>& model) const override {
        // We only test shrinking with resize(), since Vector doesn't initialize
        // fundamental types to zero after resizing.
        RC_PRE(value <= model.size());
    }

    void apply(VectorModel<T>& model) const override { model.resize(value);}

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        vector.resize(value); }
    void show(std::ostream& os) const override { os << "resize(" << value << ")"; }
};

template<typename T>
struct subscript : public VectorCommand<T> {
    int offset = *rc::gen::inRange(0, 10);

    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(offset < model.size());
    }
    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        RC_ASSERT(model[offset] == vector[offset]);
    }
    void show(std::ostream& os) const override { os << "[" << offset << "]"; }
};

template<typename T>
struct front : public VectorCommand<T> {
    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(model.size() > 0);
    }
    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        RC_ASSERT(model.front() == vector.front());
    }
    void show(std::ostream& os) const override { os << "front()"; }
};

template<typename T>
struct back : public VectorCommand<T> {
    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(model.size() > 0);
    }
    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        RC_ASSERT(model.back() == vector.back());
    }
    void show(std::ostream& os) const override { os << "back()"; }
};

template<typename T>
struct assign_range : public VectorCommand<T> {
    std::vector<T> values = *rc::gen::arbitrary<std::vector<T>>();

    void apply(VectorModel<T>& model) const override {
        model.assign(values.begin(), values.end()); }
    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        vector.assign(values.begin(), values.end());
    }
    void show(std::ostream& os) const override {
        os << "assign(values)"; }
};

template<typename T>
struct assign_fill : public VectorCommand<T> {
    int length = *rc::gen::inRange(0, 10);
    T value = *rc::gen::arbitrary<T>();
    void apply(VectorModel<T>& model) const override {
        model.assign(length, value); }
    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        vector.assign(length, value);
    }
    void show(std::ostream& os) const override {
        os << "assign(" << length << ", " << value; }
};

template<typename T>
struct push_back : public VectorCommand<T> {
    T value = *rc::gen::arbitrary<T>();

    void apply(VectorModel<T>& model) const override { model.push_back(value);}

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        vector.push_back(value); }
    void show(std::ostream& os) const override {
        os << "push_back(" << value << ")"; }
};

template<typename T>
struct pop_back : public VectorCommand<T> {
    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(model.size() > 0);
    }

    void apply(VectorModel<T>& model) const override { model.pop_back(); }

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        vector.pop_back(); }
    void show(std::ostream& os) const override {
        os << "pop_back();";
    }
};

template<typename T>
struct insert : public VectorCommand<T> {
    int offset = *rc::gen::inRange(0, 10);
    T value = *rc::gen::arbitrary<T>();

    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(offset < model.size());
    }

    void apply(VectorModel<T>& model) const override {
        model.insert(model.begin() + offset, value); }

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        auto result = vector.insert(vector.begin() + offset, value);
        VectorModel<T> model_copy(model);
        auto expected = model_copy.insert(model_copy.begin() + offset, value);
        RC_ASSERT(result - vector.begin() == expected - model_copy.begin());
    }
    void show(std::ostream& os) const override {
        os << "insert(" << offset << ", " << value << ")";
    }
};

template<typename T>
struct insert_range : public VectorCommand<T> {
    int offset = *rc::gen::inRange(0, 10);
    std::vector<T> values = *rc::gen::arbitrary<std::vector<T>>();

    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(offset < model.size());
    }

    void apply(VectorModel<T>& model) const override {
        model.insert(model.begin() + offset, values.begin(), values.end()); }

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        vector.insert(vector.begin() + offset, values.begin(), values.end()); }

    void show(std::ostream& os) const override {
        os << "insert(values)";
    }
};

template<typename T>
struct erase : public VectorCommand<T> {
    int offset = *rc::gen::inRange(0, 10);

    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(offset < model.size());
    }

    void apply(VectorModel<T>& model) const override {
        model.erase(model.begin() + offset); }

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        auto result = vector.erase(vector.begin() + offset);
        VectorModel<T> model_copy(model);
        auto expected = model_copy.erase(model_copy.begin() + offset);
        RC_ASSERT(result - vector.begin() == expected - model_copy.begin());
    }
    void show(std::ostream& os) const override {
        os << "erase(" << offset << ")";
    }
};

template<typename T>
struct erase_range : public VectorCommand<T> {
    int offset = *rc::gen::inRange(0, 10);
    int length = *rc::gen::inRange(0, 10);

    void checkPreconditions(const VectorModel<T>& model) const override {
        RC_PRE(offset + length < model.size());
    }

    void apply(VectorModel<T>& model) const override {
        model.erase(model.begin() + offset, model.begin() + offset + length); }

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        auto result = vector.erase(vector.begin() + offset,
                                   vector.begin() + offset + length);
        VectorModel<T> model_copy(model);
        auto expected = model_copy.erase(model_copy.begin() + offset,
                                         model_copy.begin() + offset + length);
        RC_ASSERT(result - vector.begin() == expected - model_copy.begin());
    }
    void show(std::ostream& os) const override {
        os << "erase(" << offset << ", " << offset + length << ")";
    }
};

template<typename T>
struct clear : public VectorCommand<T> {
    void apply(VectorModel<T>& model) const override { model.clear(); }

    void run(const VectorModel<T>& model, Vector<T>& vector) const override {
        vector.clear();
    }
    void show(std::ostream& os) const override { os << "clear()"; }
};

}

RC_GTEST_PROP(Vector, int, ()) {
    rc::check([] {
        VectorModel<int> model;
        Vector<int>* vector = new Vector<int>();
        rc::state::check(model, *vector,
                         rc::state::gen::execOneOfWithArgs<
                         size<int>,
                         resize<int>,
                         subscript<int>,
                         front<int>,
                         back<int>,
                         assign_range<int>,
                         assign_fill<int>,
                         push_back<int>,
                         pop_back<int>,
                         insert<int>,
                         insert_range<int>,
                         erase<int>,
                         erase_range<int>,
                         clear<int>
                         >());
    });
};

RC_GTEST_PROP(Vector, string, ()) {
    rc::check([] {
        VectorModel<std::string> model;
        Vector<std::string>* vector = new Vector<std::string>();
        rc::state::check(model, *vector,
                         rc::state::gen::execOneOfWithArgs<
                         size<std::string>,
                         resize<std::string>,
                         subscript<std::string>,
                         front<std::string>,
                         back<std::string>,
                         assign_range<std::string>,
                         assign_fill<std::string>,
                         push_back<std::string>,
                         pop_back<std::string>,
                         insert<std::string>,
                         insert_range<std::string>,
                         erase<std::string>,
                         erase_range<std::string>,
                         clear<std::string>
                         >());
    });
};
