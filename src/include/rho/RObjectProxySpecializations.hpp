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
#ifndef RHO_ROBJECTPROXYSPECIALIZATIONS_HPP
#define RHO_ROBJECTPROXYSPECIALIZATIONS_HPP

#include "RObjectProxy.hpp"

namespace rho {
namespace internal {

    template<>
    class RObjectProxy<VectorBase> : public RObjectProxy<> {
        // This class mimics const VectorBase.
    public:
        typedef VectorBase::size_type size_type;

        RObjectProxy() {}
        RObjectProxy(const VectorBase* value)
            : RObjectProxy<>(value) { }
        RObjectProxy(const RObjectProxy<VectorBase>& other) = default;

        // See the VectorBase API for documentation of these functions.
        const ListVector* dimensionNames() const {
            return isRObject() ? getRObject()->dimensionNames() : nullptr;
        }
        const StringVector* dimensionNames(unsigned int d) const {
            return isRObject() ? getRObject()->dimensionNames(d) : nullptr;
        }
        const IntVector* dimensions() const {
            return isRObject() ? getRObject()->dimensions() : nullptr;
        }
        const StringVector* names() const {
            return isRObject() ? getRObject()->names() : nullptr;
        }
    private:
        // For a scalar StringVector, this should return String*.
        const VectorBase* getRObject() const {
            return static_cast<const VectorBase*>(
                RObjectProxy<>::getRObject());
        }
    };

    // This class mimics const FixedVector.
    template <typename T, SEXPTYPE ST, typename Initializer>
    class RObjectProxy<FixedVector<T, ST, Initializer>, std::false_type>
        : public RObjectProxy<VectorBase> {
        typedef FixedVector<T, ST, Initializer> vector_type;
        typedef RObjectProxy<vector_type> this_type;
    public:
        typedef T value_type;

        RObjectProxy(const vector_type* value)
            : RObjectProxy<VectorBase>(value) {
        }
        RObjectProxy(value_type value);
        RObjectProxy(const this_type& other) = default;

        T operator[](size_type index) const {
            if (isRObject())
                return (*getRObject())[index];
            assert(index == 0);
            return getScalarValue();
        }

        // These are broken for scalar characters.
        const T* begin() const {
            if (isRObject()) {
                return getRObject()->begin();
            }
            return scalarBegin();
        }
        const T* end() const {
            return begin() + size();
        }
    private:
        const FixedVector<T, ST, Initializer>* getRObject() const {
            return static_cast<const FixedVector<T, ST, Initializer>*>(
                RObjectProxy<>::getRObject());
        }

        const T* scalarBegin() const;
        T getScalarValue() const { return *scalarBegin(); }
    };

    template<typename T>
    class RObjectProxy<T, std::true_type>
        : public RObjectProxy<> {
    public:
        RObjectProxy(const T* value);
        RObjectProxy(const RObjectProxy<T>& other) = default;
    };

    template<>
    inline RObjectProxy<IntVector>::RObjectProxy(int value) {
        setInteger(value);
    }

    template<>
    inline const int* RObjectProxy<IntVector>::scalarBegin() const {
        return getInteger();
    }

    template<>
    inline RObjectProxy<LogicalVector>::RObjectProxy(Logical value) {
        setLogical(static_cast<int>(value));
    }

    template<>
    inline const Logical* RObjectProxy<LogicalVector>::scalarBegin() const {
        return getLogical();
    }

    template<>
    inline RObjectProxy<StringVector>::RObjectProxy(RHandle<String> value) {
        setString(value);
    }

    template<>
    inline RHandle<String> RObjectProxy<StringVector>::getScalarValue() const {
        assert(getStorageType() == SCALAR_STRING);
        RHandle<String> handle;
        handle = const_cast<String*>(getString());
        return handle;
    }

    // Currently no RObjectProxy<StringVector>::scalarBegin() implementation.
    // Likely needs a non-trivial iterator.

    template<>
    inline RObjectProxy<RealVector>::RObjectProxy(double value) {
        setReal(value);
    }

    template<>
    inline const double* RObjectProxy<RealVector>::scalarBegin() const {
        return getReal();
    }

    template<typename T,
             bool has_pointer_rep_only = has_pointer_rep_only<T>()>
    struct DereferenceFunctions;

    template<typename T>
    struct DereferenceFunctions<T, true> {
        typedef T& reference;
        typedef const T& const_reference;
        typedef T* pointer;
        typedef const T* const_pointer;

        static pointer get(const RObjectProxy<T>& value) {
            return const_cast<pointer>(
                static_cast<const T*>(value.getPointer()));
        }
    };

    template<typename T>
    struct DereferenceFunctions<T, false> {
        typedef RObjectProxy<T>& reference;
        typedef const RObjectProxy<T>& const_reference;
        typedef RObjectProxy<T>* pointer;
        typedef const RObjectProxy<T>* const_pointer;

        static pointer get(const RObjectProxy<T>& value) {
            return const_cast<pointer>(&value);
        }
    };

}  // namespace internal

}  // namespace rho

#endif  // RHO_ROBJECTPROXYSPECIALIZATIONS_HPP
