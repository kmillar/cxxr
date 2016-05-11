/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1999-2007   The R Development Core Team.
 *  Copyright (C) 2008-2014  Andrew R. Runnalls.
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

#ifndef RHO_RVALUE_HPP
#define RHO_RVALUE_HPP

/** @file RValue.hpp
 *
 * @brief Class rho::RValue.
 */

namespace rho {

class RObject;

template<class T = RObject>
class RValue;

}

#include <stdint.h>
#include "Rinternals.h"
#include "Defn.h"
#include "rho/ComplexVector.hpp"
#include "rho/IntVector.hpp"
#include "rho/LogicalVector.hpp"
#include "rho/RObject.hpp"
#include "rho/RObjectProxy.hpp"
#include "rho/RObjectProxySpecializations.hpp"
#include "rho/RawVector.hpp"
#include "rho/RealVector.hpp"
#include "rho/SEXP_downcast.hpp"
#include "rho/StringVector.hpp"
#include "rho/Symbol.hpp"


namespace rho {

/** @brief Efficiently store R objects.
 *
 * RValue<> is analagous to 'const RObject*' or SEXP, but is capable of storing
 * scalar values directly using tagged pointers, instead of having to allocate
 * an RObject on the heap to store the value.  This is (mostly) transparent to
 * code using the RValue.
 *
 * TODO: support storing RValue in a GCEdge.
 */
template<class T>
class RValue
{
public:
    typedef T value_type;
    typedef const T const_value_type;
    typedef typename internal::DereferenceFunctions<T>::reference reference;
    typedef typename internal::DereferenceFunctions<T>::const_reference
        const_reference;
    typedef typename internal::DereferenceFunctions<T>::pointer pointer;
    typedef typename internal::DereferenceFunctions<T>::const_pointer
        const_pointer;

    /* implicit */ RValue(const T* object = nullptr) : m_value(object) { }

    /** @brief 'Upcasting' copy constructor. */
    template<typename U, typename FLAG
               = typename std::enable_if<std::is_base_of<T, U>::value>::type>
    RValue(const RValue<U>& other) : m_value(other.m_value) {
    }

    /** @brief Upcasts for lvalue references. */
    template<typename U, typename FLAG
               = typename std::enable_if<std::is_base_of<T, U>::value>::type>
    operator RValue<U>&() {
        // When passing lvalue references, we don't want casts to create
        // temporaries, otherwise the temporary will get modified instead of the
        // original value.
        return *reinterpret_cast<RValue<U>*>(this);
    }

    /** @brief Named constructor for scalar values. */
    template<typename x
        = std::enable_if<!internal::has_pointer_rep_only<T>(), T>>
    static RValue<T> scalar(typename x::type::value_type value) {
        return RValue<T>(internal::RObjectProxy<T>(value));
    }

    /** @brief Implicit cast to T*
     *
     * @note Implicit casts from RValue<T> to T* are allowed only in the case
     *   where the pointer representation is the only valid one.
     *   For all other cases, that cast is a potentially expensive operation.
     */
    template<typename U = T,
             typename = typename std::enable_if<
                 internal::has_pointer_rep_only<U>()>::type>
    operator /*const*/ T*() const;

    bool isNull() const {
        return m_value.isNull();
    }

    bool isRObject() const {
        return m_value.isRObject();
    }

    /** @brief Dereferencing operator.
     *
     * @note Depending on the type of T, the return value may be a T& or
     *    an RObjectProxy<T>&.  The interfaces of the two types are similar.
     */
    reference operator*() const { return *operator->(); }

    /** @brief Dereferencing operator.
     *
     * @note Depending on the type of T, the return value may be a T* or
     *    an RObjectProxy<T>*.  The interfaces of the two types are similar.
     */
    pointer operator->() const {
        return internal::DereferenceFunctions<T>::get(m_value);
    }

    /** @brief Check if two RValues refer to the same heap object. */
    bool operator==(const RValue<>& other) {
        return m_value.isPointer() && m_value == other.m_value;
    }
    bool operator!=(const RValue<>& other) {
        return !(*this == other);
    }

    // Downcasts.
    template<typename U,
             typename FLAG = std::enable_if<std::is_base_of<U, T>::value
                                            || std::is_base_of<T, U>::value>>
    RValue<U> down_cast(bool allow_null = true) const {
        if (!allow_null && m_value.isNull()) {
            SEXP_downcast_error("NULL", U::staticTypeName());
        }
#ifndef UNCHECKED_SEXP_DOWNCAST
        // SEXPTYPE type = (*this)->sexptype();
        // if (!U::validSexptype(type)) {
        //     SEXP_downcast_error(Rf_type2char(type),
        //                         U::staticTypeName());
        // }
#endif
        return RValue<U>(m_value);
    }

    // template<typename U,
    //          typename FLAG = std::enable_if<std::is_base_of<U, T>::value
    //                                         || std::is_base_of<T, U>::value>>
    // RValue<U> dynamic_down_cast() const {
    //     if (U::validSexptype((*this)->sexptype())) {
    //         return RValue<U>(m_value);
    //     }
    //     return RValue<U>(nullptr);
    // }

    T* mutable_copy() const;

    static RObject* deprecated_asRObject(RValue<> object);

private:
    internal::RObjectProxy<T> m_value;

    explicit RValue(const internal::RObjectProxy<> &value)
        : m_value(static_cast<const internal::RObjectProxy<T>&>(value)) {
    }

    template<class U>
    friend class RValue;

    friend class internal::RObjectProxy<T>;
};

template <typename PtrOut, typename InputType>
typename std::enable_if<std::is_pointer<PtrOut>::value,
                        RValue<typename std::remove_pointer<PtrOut>::type>>
    ::type
SEXP_downcast(const RValue<InputType>& s, bool allow_null = true)
{
    typedef typename std::remove_pointer<PtrOut>::type OutType;
    return s.template down_cast<OutType>(allow_null);
}

// Allow use of SEXP_downcast<RObject&>(foo) on lvalue RValue objects.
template <typename RefOut, typename InputType>
typename std::enable_if<std::is_reference<RefOut>::value,
                        RValue<typename std::remove_reference<RefOut>::type>&>
    ::type
SEXP_downcast(RValue<InputType>& s, bool allow_null = true)
{
    typedef typename std::remove_reference<RefOut>::type OutType;
    // Check the cast.
    SEXP_downcast<OutType*>(s);
    return reinterpret_cast<RValue<OutType>&>(s);
}

// Convenience wrapper around RValue::dynamic_down_cast.
template<typename PtrOut, typename TypeIn>
RValue<typename std::remove_pointer<PtrOut>::type>
SEXP_dynamic_cast(RValue<TypeIn> s) {
    typedef RValue<typename std::remove_pointer<PtrOut>::type> return_type;
    return s.template dynamic_down_cast<typename return_type::value_type>();
}

template<typename T>
template<typename, typename>
inline RValue<T>::operator T*() const {
    static_assert(internal::has_pointer_rep_only<T>(),
                  "Implicit conversion to pointer not supported.");
    return const_cast<T*>(internal::DereferenceFunctions<T>::get(m_value));
}

inline RValue<>
internal::RObjectProxy<>::evaluate(Environment* env) const
{
    if (!isRObject()) {
        return RValue<>(*this);
    }
    return const_cast<RObject*>(getRObject())->evaluate(env);
}

inline RValue<>
internal::RObjectProxy<RObject>::getAttribute(const Symbol* name) const {
    if(!isRObject())
        return RValue<>();
    return getRObject()->getAttribute(name);
}

inline void
internal::RObjectProxy<RObject>::maybeTraceMemory(RValue<> src)
{
    if (isRObject() && src.isRObject()) {
        getRObject()->maybeTraceMemory(src->getRObject());
    }
}

inline RValue<>
internal::RObjectProxy<RObject>::clone() const {
    return isRObject() ? RValue<>(getRObject()->clone()) : RValue<>(*this);
}

// Compatibility functions to allow code that used SEXP values to be easily
// converted to use RValue<>.
inline bool Rf_isNull(RValue<> object) {
    return object.isNull();
}

inline bool Rf_isSymbol(RValue<> object) {
    return object->sexptype() == SYMSXP;
}

inline bool Rf_isReal(RValue<> object) {
    return object->sexptype() == REALSXP;
}

inline bool Rf_isComplex(RValue<> object) {
    return object->sexptype() == CPLXSXP;
}

inline bool Rf_isExpression(RValue<> object) {
    return object->sexptype() == EXPRSXP;
}

inline bool Rf_isEnvironment(RValue<> object) {
    return object->sexptype() == ENVSXP;
}

inline bool Rf_isString(RValue<> object) {
    return object->sexptype() == STRSXP;
}

inline bool Rf_isObject(RValue<> object) {
    return object->hasClass();
}

inline int Rf_asLogical(RValue<> object) {
    return static_cast<int>(object->asScalarLogical());
}

inline bool Rf_asLogicalNoNA(RValue<> object, Expression* call) {
    return object->asScalarLogicalNoNA(call);
}

inline int Rf_asInteger(RValue<> object) {
    return object->asScalarInteger();
}
inline int Rf_asReal(RValue<> object) {
    return object->asScalarReal();
}

inline PairList* ATTRIB(RValue<> object) {
    return const_cast<PairList*>(object->attributes());
}

inline bool OBJECT(RValue<> object) {
    return Rf_isObject(object);
}

inline SEXPTYPE TYPEOF(RValue<> object) {
    return object->sexptype();
}

inline bool IS_S4_OBJECT(RValue<> object) {
    return object->isS4Object();
}

inline void UNSET_S4_OBJECT(RValue<> object) {
    object->setS4Object(false);
}

inline R_xlen_t Rf_length(RValue<> object) {
    return object->size();
}

inline R_xlen_t length(RValue<> object) {
    return Rf_length(object);
}

inline R_xlen_t XLENGTH(RValue<> vec) {
    return vec->size();
}

template<class T>
inline int* LOGICAL(RValue<T>& vec) {
    // This is complicated by the fact that we may be returning a pointer
    // into the RValue.  To prevent returning a pointer to a local variable,
    // we can't use a local copy of vec (which SEXP_downcast would create).
    // Instead we use references everywhere.
    //
    // Similarly, this function is templated to eliminate compiler-generated
    // temporaries when converting from RValue<T> to RValue<RObject>.
    RValue<LogicalVector>& logicals = SEXP_downcast<LogicalVector&>(vec);
    return const_cast<int*>(reinterpret_cast<const int*>(logicals->begin()));
}

template<class T>
inline const int* LOGICAL(const RValue<>& vec) {
    return LOGICAL(const_cast<RValue<>&>(vec));
}

template<class T>
inline int* INTEGER(RValue<T>& vec) {
    RValue<IntVector>& ints = SEXP_downcast<IntVector&>(vec);
    return const_cast<int*>(ints->begin());
}

template<class T>
inline const int* INTEGER(const RValue<T>& vec) {
    return INTEGER(const_cast<RValue<>&>(vec));
}

inline Rbyte* RAW(RValue<> vec) {
    RValue<RawVector> raw = SEXP_downcast<RawVector*>(vec);
    return raw->begin();
}

inline const double* REAL(RValue<> vec) {
    RValue<RealVector>& reals = SEXP_downcast<RealVector&>(vec);
    return reals->begin();
}

inline Complex* COMPLEX(RValue<> vec) {
    auto complex = SEXP_downcast<ComplexVector*>(vec);
    return complex->begin();
}

inline String* STRING_ELT(RValue<> vec, R_xlen_t i) {
    auto strings = SEXP_downcast<StringVector*>(vec);
    return (*strings)[i].get();
}

inline RValue<> VECTOR_ELT(RValue<> vec, R_xlen_t i) {
    RValue<ListVector> list = SEXP_downcast<ListVector*>(vec);
    return (*list)[i].get();
}

inline const char* R_CHAR(RValue<> value) {
    RValue<String> chars = SEXP_downcast<String*>(value);
    return chars->begin();
}

inline bool Rf_inherits(RValue<> object, const char* name) {
    return object->inherits(name);
}

inline RValue<> Rf_getAttrib(RValue<> object, RValue<> attribute) {
    return object->getAttribute(SEXP_downcast<Symbol*>(attribute));
}
inline RValue<> Rf_getAttrib(RValue<> object, SEXP attribute) {
    return object->getAttribute(SEXP_downcast<Symbol*>(attribute));
}
inline RValue<> Rf_getAttrib(SEXP object, RValue<> attribute) {
    return object->getAttribute(SEXP_downcast<Symbol*>(attribute));
}

inline int NAMED(RValue<> object) {
    return object->named();
}

template<typename T>
inline RValue<T> Rf_duplicate(RValue<T> input) {
    return input->clone();
}

}  // namespace rho

#endif  // RHO_RVALUE_HPP
