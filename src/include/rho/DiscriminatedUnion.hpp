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

/** @file DiscriminatedUnion.hpp
 * @brief Class rho::DiscriminatedUnion.
 */
#ifndef RHO_DISCRIMINATED_UNION_HPP
#define RHO_DISCRIMINATED_UNION_HPP

#include <cassert>
#include <cstdint>
#include <utility>

namespace rho {
    /** @brief Discriminated union.
     *
     * DiscriminatedUnion is a discriminated union capable of storing two
     * different types of pointer, 65536 different types of integer and most
     * double values, while fitting into 64 bits.
     *
     * @note The pointer API of this class is neither type safe nor const
     *   correct.  It should be wrapped in a class that corrects these issues.
     */
    class DiscriminatedUnion {
    public:
        void setPointer1(void* node, bool flag = false) {
            if (!node && !flag) {
                m_value.bits = s_pointer_1_null_value;
                return;
            }

            uint64_t bits = reinterpret_cast<uintptr_t>(node);
            bits = bits
                // Set the lower bits to store the information required to clear
                // the tag in the upper bits and later recover the original
                // values.
                | ((bits >> 60) & 0xe)
                // Store the flag.
                | flag;
            // Set the upper bits to the required pattern.
            m_value.bits = bits ^ (bits << 60);
        }
        bool isPointer1() const {
            return getStorageType() == POINTER_1 || isPointer1Null();
        }
        bool isNonNullPointer1() const {
            return getStorageType() == POINTER_1;
        }
        std::pair<void*, bool> getPointer1() const {
            assert(isPointer1());
            if (isPointer1Null())
                return std::make_pair(nullptr, false);
            void* pointer = recoverPointer();
            bool flag = m_value.bits & 0x1;
            return std::make_pair(pointer, flag);
        }

        void setPointer2(void* node) {
            uint64_t bits = reinterpret_cast<uintptr_t>(node);
            // Set the lower bits to store the information required to set the
            // tag in the upper bits and later recover the original values.
            bits = bits | ((bits ^ s_pointer_2_tag) >> 60);
            // Set the upper bits to the required pattern.
            m_value.bits = bits ^ (bits << 60);
        }
        bool isPointer2() const {
            return getStorageType() == POINTER_2;
        }
        bool isNonNullPointer2() const {
            return getStorageType() == POINTER_2
                && m_value.bits != s_pointer_2_null_value;
        }
        void* getPointer2() const {
            assert(isPointer2());
            return recoverPointer();
        }

        bool isEitherPointer() const {
            return isPointer1() || isPointer2();
        }
        bool isEitherPointerNonNull() const {
            return isNonNullPointer1() || isNonNullPointer2();
        }
        void* getEitherPointer() const {
            assert(isEitherPointer());
            if (isPointer1Null())
                return nullptr;
            return recoverPointer();
        }

        void setInteger(uint16_t tag, int value) {
            m_value.signed_bits = static_cast<int>(value) | makeIntegerTag(tag);
        }
        bool isInteger(uint16_t tag) const {
            return (m_value.signed_bits & 0xffffffff00000000)
                == makeIntegerTag(tag);
        }

        // These functions return references so that callers can get a pointer
        // to the value if desired.
        const int& getInteger(uint16_t tag) const {
            assert(isInteger(tag));
            return getLower32Bits();
        }
        int& getInteger(uint16_t tag) {
            assert(isInteger(tag));
            return getLower32Bits();
        }

        void setDouble(double value) {
            assert(isStorableDoubleValue(value));
            m_value.double_value = value;
        }
        bool isDouble() const {
            return getStorageType() == DOUBLE;
        }
        const double& getDouble() const {
            assert(isDouble());
            return m_value.double_value;
        }
        // No function returning a double lvalue, as not all values are legal.

        //* A value is storable if we can store and retrieve it.
        static bool isStorableDoubleValue(double d) {
            DiscriminatedUnion test_value;
            test_value.m_value.double_value = d;
            return test_value.isDouble();
        }

        enum StorageType : unsigned char {
            INTEGER,
            DOUBLE,
            POINTER_1,
            POINTER_1_NULL,
            POINTER_2
        };

        StorageType getStorageType() const {
            // The flag patterns are up to five bits long, so we can use them to
            // index into a 32 element array.
            static constexpr StorageType s_storage_type[32] = {
                POINTER_1, POINTER_1, POINTER_1, POINTER_1,
                DOUBLE, DOUBLE, DOUBLE, DOUBLE,
                DOUBLE, DOUBLE, DOUBLE, DOUBLE,
                POINTER_2, POINTER_2, INTEGER, DOUBLE,
                POINTER_1, POINTER_1, POINTER_1, POINTER_1,
                DOUBLE, DOUBLE, DOUBLE, DOUBLE,
                DOUBLE, DOUBLE, DOUBLE, DOUBLE,
                POINTER_2, POINTER_2, POINTER_1_NULL, DOUBLE,
            };
            if (isZeroDouble()) {
                return DOUBLE;
            }
            static_assert(sizeof(void*) == 8, "rho requires 64 bit pointers");
            return s_storage_type[m_value.bits >> 59];
        }

        // If candidate_pointer might be an encoded pointer, return the pointer
        // that it might be.  Else return null.
        static void* interpret_possible_pointer(void* candidate_pointer) {
            DiscriminatedUnion value;
            value.m_value.pointer = candidate_pointer;
            if (value.isEitherPointer())
                return value.getEitherPointer();
            return nullptr;
        }
    private:
        union tagged_pointer {
            void* pointer;
            uint64_t bits;
            int64_t signed_bits;
            double double_value;
        };
        tagged_pointer m_value;

        // Bits 59-63 determine the type of the encoded value.

        // x00xx - Pointer 1, unless all the bits are zero, in which case it's a
        //           double with a value of zero.
        //           The pointer value is recovered by xor-ing bits 0-3 into
        //           bits 59-62 and clearing the bottom 3 bits.
        //           Cannot be NULL.
        //           The lowest bit is used as an additional flag bit that
        //           client code can use to store information.
        // x01xx - Double. Native format
        // x10xx - Double. Native format

        // x110x - Pointer 2.  The pointer value is recovered in the same way
        //           as for pointer 1.  No flag bit.
        // 01110 - Integer. Index held in bits 32-47, value held in the lower
        //           32 bits.
        // 11110 - Pointer 1 NULL value..
        // x1111 - Double. Native format, includes +/- Inf and NaN.
        //
        // This encoding is useful because it allows most double values to be
        // stored in their normal format (the exceptions being very large and
        // very small numbers), while also handling the full range of pointers,
        // integers and logicals.

        static constexpr uintptr_t s_pointer_1_tag = 0;
        static constexpr uintptr_t s_pointer_1_null_tag
            = static_cast<uintptr_t>(0x1e) << 59;
        static constexpr uintptr_t s_pointer_2_tag
            = static_cast<uintptr_t>(0xc) << 59;
        static constexpr uintptr_t s_integer_tag
            = static_cast<uintptr_t>(0xe) << 59;

        static constexpr uintptr_t s_pointer_1_null_value
            = s_pointer_1_null_tag ^ (s_pointer_1_null_tag >> 60);
        static constexpr uintptr_t s_pointer_2_null_value
            = s_pointer_2_tag ^ (s_pointer_2_tag >> 60);

        bool isZeroDouble() const {
            return m_value.bits == 0;
        }
        bool isPointer1Null() const {
            return getStorageType() == POINTER_1_NULL;
        }

        void* recoverPointer() const {
            return reinterpret_cast<void*>((m_value.bits & ~0x7)
                                           ^ (m_value.bits << 60));
        }

        int& getLower32Bits() {
            static_assert(sizeof(int) == 4,
                          "rho expects 32 bit integers.");
#ifdef WORDS_BIGENDIAN
            return *reinterpret_cast<int*>(&m_value.signed_bits) + 1;
#else
            return *reinterpret_cast<int*>(&m_value.signed_bits);
#endif
        }

        const int& getLower32Bits() const {
            return const_cast<DiscriminatedUnion*>(this)->getLower32Bits();
        }

        static uint64_t makeIntegerTag(uint16_t index) {
            return s_integer_tag | (static_cast<uint64_t>(index) << 32);
        }
    };
}  // namespace rho
#endif  // RHO_DISCRIMINATED_UNION_HPP
