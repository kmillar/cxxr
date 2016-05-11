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
#ifndef RHO_ROBJECTPROXY_HPP
#define RHO_ROBJECTPROXY_HPP

#include <type_traits>

#include "rho/Expression.hpp"
#include "rho/IntVector.hpp"
#include "rho/LogicalVector.hpp"
#include "rho/RealVector.hpp"
#include "rho/RValue.hpp"
#include "rho/StringVector.hpp"

namespace rho {

namespace internal {
    template<class T>
    constexpr bool has_pointer_rep_only() {
        return !(std::is_base_of<T, LogicalVector>::value
                 || std::is_base_of<T, StringVector>::value
                 || std::is_base_of<T, IntVector>::value
                 || std::is_base_of<T, RealVector>::value);
    }

    /**
     * @brief RObjectProxy is a helper class for RValue and shouldn't be used
     *    directly.
     */
    template<typename T = RObject,
             typename tag
                 = std::integral_constant<bool, has_pointer_rep_only<T>()>>
        class RObjectProxy;

    static constexpr uintptr_t shiftbits(uintptr_t value) {
        return value << 60;  /* Must match s_flag_bits_location below */
    }

    static constexpr uintptr_t shift_3_bits(uintptr_t value) {
        return (value & 0x7) << 61;
    }

    static constexpr uintptr_t unshift_3_bits(uintptr_t value) {
        return (value >> 61) & 0x7;
    }

    template<>
    class RObjectProxy<RObject> {
        // This class mimics const RObject.  All other RObjectProxy types
        // inherit from it.
    public:
        RObjectProxy() : RObjectProxy(nullptr) {}
        RObjectProxy(const RObject* value) {
            setPointer(value, s_robject_tag);
        }
        RObjectProxy(const RObjectProxy<RObject>& other) = default;

        // See the RObject API for documentation of these functions.
        SEXPTYPE sexptype() const {
            return getSexpType();
        }

        const char* typeName() const {
            return Rf_type2char(sexptype());
        }

        RValue<> evaluate(Environment* env) const;

        const PairList* attributes() const {
            // CHARSXP never has attributes, so we only need to check RObject.
            return isRObject() ? getRObject()->attributes() : nullptr;
        }

        size_t size() const {
            if (isNull())
                return 0;
            if (isPointer())
                return SEXP_downcast<const VectorBase*>(getPointer())->size();
            return 1;
        }

        void clearAttributes() {
            if (isRObject())
                const_cast<RObject*>(getRObject())->clearAttributes();
        }

        RValue<> getAttribute(const Symbol* name) const;

        bool hasAttributes() const {
            return isRObject() ? getRObject()->hasAttributes() : false;
        }

        bool hasClass() const {
            return isRObject() ? getRObject()->hasClass() : false;
        }

        bool isS4Object() const {
            return isRObject() ? getRObject()->isS4Object() : false;
        }

        void setS4Object(bool on) {
            if (isRObject())
                getRObject()->setS4Object(on);
            else {
                assert(!on);
            }
        }

        int named() const {
            return isPointer() ? NAMED(const_cast<SEXP>(getPointer()))
                : NAMEDMAX;
        }

        RValue<> clone() const;

        void maybeTraceMemory(RValue<> src);

        void detachReferents() {
            if (isRObject())
                const_cast<RObject*>(getRObject())->detachReferents();
        }

        void visitReferents(GCNode::const_visitor* v) const {
            if (isRObject())
                getRObject()->visitReferents(v);
        }

        // This stuff isn't in the RObject API.
        bool isNull() const {
            return getFlagBits() == s_nil_tag;
        }

#if 0
        // Needs renaming.
        template<class U>
        RValue<U> as(bool allow_null = true) const;
#if 0
        {
#ifndef UNCHECKED_SEXP_DOWNCAST
            U* u;
            if (sexptype() != U::sexptype()) {
                SEXP_downcast_error(Rf_type2char(sexptype()),
                                    U::staticTypeName());
            }
#endif
            if (!allow_null && isNull()) {
                SEXP_downcast_error("NULL", U::staticTypeName());
            }
            return RObjectProxy<U>(m_value);
        }
#endif
#endif

        Logical asScalarLogical() const {
            switch (getStorageType()) {
                case NIL:
                    return NA_LOGICAL;
                case SCALAR_LOGICAL:
                    return *getLogical();
                case SCALAR_INTEGER:
                    return Logical(*getInteger());
                case SCALAR_REAL: {
                    double value = *getReal();
                    return ISNAN(value) ? NA_LOGICAL : value != 0;
                }
                case ROBJECT:
                    if (isRealZero()) {
                        return false;
                    }
                    FALLTHROUGH_INTENDED;
                case SCALAR_STRING:
                    return Rf_asLogical(const_cast<RObject*>(getPointer()));
            }
        }

        bool asScalarLogicalNoNA(Expression* call) const {
            Logical value = Logical::NA();
            switch (getStorageType()) {
                case SCALAR_LOGICAL:
                    value = *getLogical();
                    break;
                case SCALAR_INTEGER:
                    value = Logical(*getInteger());
                    break;
                case SCALAR_REAL: {
                    double dbl_value = *getReal();
                    value = ISNAN(dbl_value) ? NA_LOGICAL : dbl_value != 0;
                    break;
                }
                case ROBJECT:
                    if (isRealZero()) {
                        return false;
                    }
                    FALLTHROUGH_INTENDED;
                case NIL:
                case SCALAR_STRING:
                    return Rf_asLogicalNoNA(const_cast<RObject*>(getPointer()),
                                            call);
            }
            if (value.isNA()) {
                return Rf_asLogicalNoNA(R_LogicalNAValue, call);
            }
            return value.isTrue();
        }

        int asScalarInteger() const {
            switch (getStorageType()) {
                case NIL:
                    return NA_INTEGER;
                case SCALAR_LOGICAL:
                    return static_cast<int>(*getLogical());
                case SCALAR_INTEGER:
                    return *getInteger();
                case SCALAR_REAL: {
                    int warn = 0;
                    int result = Rf_IntegerFromReal(*getReal(), &warn);
                    if (warn)
                        Rf_CoercionWarning(warn);
                    return result;
                }

                case ROBJECT:
                    if (isRealZero()) {
                        return 0;
                    }
                    FALLTHROUGH_INTENDED;
                case SCALAR_STRING:
                    return Rf_asInteger(const_cast<RObject*>(getPointer()));
            }
        }

        double asScalarReal() const {
            switch (getStorageType()) {
                case NIL:
                    return NA_REAL;
                case SCALAR_LOGICAL: {
                    return static_cast<double>(*getLogical());
                }
                case SCALAR_INTEGER: {
                    int value = *getInteger();
                    return value == NA_INTEGER ? NA_REAL : value;
                }
                case SCALAR_REAL:
                    return *getReal();
                case ROBJECT:
                    if (isRealZero()) {
                        return 0;
                    }
                    FALLTHROUGH_INTENDED;
                case SCALAR_STRING:
                    return Rf_asReal(const_cast<RObject*>(getPointer()));
            }
        }

        bool inherits(const char* name) {
            if (!isRObject())
                return false;
            return ::Rf_inherits(getRObject(), name);
        }
    protected:

        bool isScalarReal() const {
            return getStorageType() == SCALAR_REAL;
        }

        // void setPointer(const RObject* p) {
        //     m_value.pointer = p;
        //     assert(isRObject());
        // }

        void setInteger(int i) {
            m_value.signed_bits = (i | s_int_tag);
        }

        static const uintptr_t s_mask_32_bit = 0xffffffff;

        int* getInteger() {
            assert(getStorageType() == SCALAR_INTEGER);
            return getLower32Bits();
        }

        const int* getInteger() const {
            return const_cast<RObjectProxy<RObject>*>(this)->getInteger();
        }

        void setLogical(Logical value) {
            static_assert(sizeof(Logical) == 4,
                          "rho expects logical values to be 32 bits");
            m_value.signed_bits = (static_cast<int>(value) | s_logical_tag);
        }

        Logical* getLogical() {
            assert(getStorageType() == SCALAR_LOGICAL);
            return reinterpret_cast<Logical*>(getLower32Bits());
        }

        const Logical* getLogical() const {
            return const_cast<RObjectProxy<RObject>*>(this)->getLogical();
        }

        void setString(String* value) {
            setPointer(value, s_string_tag);
        }

        const String* getString() const {
            assert(getStorageType() == SCALAR_STRING);
            return SEXP_downcast<const String*>(getPointer());
        }

        void setReal(double real) {
            m_value.double_value = real;

            // Usually the real value can be stored directly (the main
            // exceptions are very large or very small values).  Check that it
            // worked and recover if it failed.
            if (real != 0 && getStorageType() != SCALAR_REAL) {
                setPointer(Rf_ScalarReal(real), s_robject_tag);
            }
        }

        // Access to real values is read-only since only a subset of reals can
        // be stored in the RObjectProxy representation.
        const double* getReal() const {
            assert(m_value.double_value == 0
                   || getStorageType() == SCALAR_REAL);
            return &m_value.double_value;
        }

        // RObject or scalar string.
        // TODO: should this return true for NULL?
        bool isPointer() const {
            if (isRealZero())
                return false;
            // Check bits 61 and 62.
            return (m_value.bits & s_any_ptr_flag_bitsmask) == 0;
        }

        RObject* getPointer() {
            assert(isPointer() || isNull());
            return reinterpret_cast<RObject*>(
                (m_value.bits & ~0x7) ^ shift_3_bits(m_value.bits));
        }

        // const version of the previous function.
        const RObject* getPointer() const {
            return const_cast<RObjectProxy<RObject>*>(this)->getPointer();
        }

        static bool isPointer(void* p) {
            return reinterpret_cast<RObjectProxy<>*>(&p)->isPointer();
        }
        static RObject* getPointer(void* p) {
            return reinterpret_cast<RObjectProxy<>*>(&p)->getPointer();
        }

        // NULL returns false for isRObject().
        bool isRObject() const {
            if (m_value.bits == 0)
                return false;
            // Check bits 61 to 63.
            return (m_value.bits & s_top_3_bits_mask) == s_robject_tag;
        }

        RObject* getRObject() {
            assert(isRObject());
            return getPointer();
        }

        // const version of the previous function.
        const RObject* getRObject() const {
            assert(isRObject());
            return getPointer();
        }

        void setPointer(const RObject* value, uint64_t tag) {
            uintptr_t bits = reinterpret_cast<uintptr_t>(value);
            assert((bits & 0x7) == 0);
            m_value.bits = (bits & ~s_top_3_bits_mask)
                | tag
                | unshift_3_bits(bits ^ tag);
        }

    private:
        union tagged_pointer {
            const RObject* pointer;
            uint64_t bits;
            int64_t signed_bits;
            double double_value;
        };
        tagged_pointer m_value;

        // Bits 60-63 determine the type of the encoded value:
        //  000x - RObject*, unless all the bits are zero, in which case it's a
        //         real with a value of zero.
        //         The pointer value is recovered by xor-ing bottom 3 bits into
        //         bits 61-63 and clearing the bottom three bits.
        //  001x - Real.  Native format.
        //  010x - Real.  Native format.
        //  0110 - logical.  Value held in the lower 32 bits.
        //  0111 - int.  Value held in the lower 32 bits.
        //  100x - Scalar String*.  Bits stored in the same way as for RObject*.
        //  101x - Real.  Native format.
        //  110x - Real.  Native format.
        //  1110 - Null.  Bottom 3 bits are set, so getPointer(Null) => nullptr.
        //  1111 - Real.  Native format.

        // This encoding is useful because it allows most real values to be
        // stored in their normal format (the exceptions being very large and
        // very small numbers), while also handling the full range of pointers
        // to RObjects, Strings, integers, logicals and null.
        // Furthermore, on machines where the top 16 bits of a pointer are all
        // zero, RObjects are stored in their native encoding as well.
        static constexpr int s_flag_bits_location = 60;

        static constexpr uintptr_t shift(uintptr_t pointer) {
            return pointer << s_flag_bits_location; }

        // bits 60 to 63.
        static constexpr uintptr_t s_flag_bits_mask = shiftbits(0xf);
        // bits 61 to 63.
        static constexpr uintptr_t s_top_3_bits_mask = shiftbits(0xe);
        // bits 61 and 62.
        static constexpr uintptr_t s_any_ptr_flag_bitsmask = shiftbits(0x6);

        static constexpr uintptr_t s_robject_tag = 0;
        static constexpr uintptr_t s_logical_tag = shiftbits(0x6);
        static constexpr uintptr_t s_int_tag     = shiftbits(0x7);
        static constexpr uintptr_t s_string_tag  = shiftbits(0x8);
        static constexpr uintptr_t s_nil_tag     = shiftbits(0x14);

        // Returns the contents of bits 60 to 63.
        uintptr_t getFlagBits() const {
            static_assert(sizeof(void*) == 8, "rho requires 64 bit pointers");
            return m_value.bits & s_flag_bits_mask;
        }

        enum StorageType : unsigned char {
            NIL,
            SCALAR_LOGICAL,
            SCALAR_INTEGER,
            SCALAR_REAL,
            SCALAR_STRING,
            ROBJECT,
        };

        StorageType getStorageType() const {
            // The flag patterns are up to four bits long, so we can use them to
            // index into a 16 element array.
            static constexpr StorageType s_storage_type[16] = {
                ROBJECT, ROBJECT, SCALAR_REAL, SCALAR_REAL,
                SCALAR_REAL, SCALAR_REAL, SCALAR_LOGICAL, SCALAR_INTEGER,
                SCALAR_STRING, SCALAR_STRING, SCALAR_REAL, SCALAR_REAL,
                SCALAR_REAL, SCALAR_REAL, NIL, SCALAR_REAL
            };
            return s_storage_type[getFlagBits() >> s_flag_bits_location];
        }

        static constexpr unsigned char UNDETERMINED_SEXPTYPE = -1;

        static constexpr unsigned char s_sexp_type[6] = {
            NILSXP, LGLSXP, INTSXP, REALSXP, STRSXP, UNDETERMINED_SEXPTYPE
        };

        SEXPTYPE getSexpType() const {
            StorageType storage_type = getStorageType();
            if (storage_type != ROBJECT) {
                return static_cast<SEXPTYPE>(s_sexp_type[storage_type]);
            }
            if (isRealZero())
                return REALSXP;
            return getRObject()->sexptype();
        }

        bool isRealZero() const {
            return m_value.bits == 0;
        }

        int* getLower32Bits() {
            static_assert(sizeof(int) == 4,
                          "rho expects 32 bit integers.");
#ifdef WORDS_BIGENDIAN
            return reinterpret_cast<int*>(&m_value.signed_bits) + 1;
#else
            return reinterpret_cast<int*>(&m_value.signed_bits);
#endif
        }

        RObjectProxy(tagged_pointer value) {
            m_value = value;
        }

        RValue<> evaluateRobject(Environment* env) const;
        RValue<> getAttributeForRObject(const Symbol* name) const;

        template<typename T, typename FLAGS>
        friend class RObjectProxy;

        template<class T>
        friend class ::rho::RValue;

        template<class T, bool b>
        friend struct DereferenceFunctions;

        friend class ::rho::GCStackRootBase;
    };
}  // namespace internal
}  // namespace rho

#endif  // RHO_ROBJECTPROXY_HPP
