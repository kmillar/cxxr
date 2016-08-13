/*
 *  R : A Computer Language for Statistical Data Analysis
 *  Copyright (C) 1995, 1996  Robert Gentleman and Ross Ihaka
 *  Copyright (C) 1999-2006   The R Development Core Team.
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

/** @file GCValue.hpp
 * @brief Class rho::GCValue.
 */
#ifndef RHO_GCVALUE_HPP
#define RHO_GCVALUE_HPP

#include "rho/DiscriminatedUnion.hpp"
#include "rho/GCNode.hpp"
#include "rho/Logical.hpp"

namespace rho {
    class String;
    template<typename> class GCStackRoot;

    template<typename T> class GCValue;

    namespace internal {
        void ensureReachable(const void*);
    }

    /** @brief GC-compatible discriminated union.
     *
     * GCValue is a discriminated union capable of storing any of
     * GCNode*, String*, int, Logical and most double values.
     *
     * GCEdge stores GCValue types.
     */
    template<>
    class GCValue<GCNode> {
    public:
        /* implicit */ GCValue(const GCNode* value) {
            setGCNode(value);
        }

        GCValue(const GCValue<GCNode>&) = default;

        /* implicit */ operator GCNode*() const {
            return getGCNode();
        }

        bool operator==(const GCValue<GCNode>& other) const {
            return getGCNode() == other.getGCNode();
        }

        /** @brief Informs the memory manager that any object this object
         *    references must not be deleted prior to this call.
         */
        void ensureReachable() const {
            if (isNonNullGCNodeOrString())
                internal::ensureReachable(getGCNodeOrString());
        }
    protected:
        //@ Store any kind of GCNode.
        void setGCNode(const GCNode* node) {
            m_value.setPointer1(const_cast<GCNode*>(node));
        }
        bool isGCNode() const {
            return m_value.isPointer1();
        }
        bool isNonNullGCNode() const {
            return m_value.isNonNullPointer1();
        }
        GCNode* getGCNode() const {
            return static_cast<GCNode*>(m_value.getPointer1().first);
        }

        //@ Store a single String*.
        void setString(const String* value) {
            m_value.setPointer2(const_cast<String*>(value));
        }
        bool isString() const {
            return m_value.isPointer2();
        }
        bool isNonNullString() const {
            return m_value.isNonNullPointer2();
        }
        const String* getString() const {
            return static_cast<const String*>(m_value.getPointer2());
        }

        /** Code that deals with memory management can treat the GCNode and
         * String values the same.  Some utility functions to make that easier.
         */
        bool isGCNodeOrString() const {
            return m_value.isEitherPointer();
        }
        bool isNonNullGCNodeOrString() const {
            return m_value.isEitherPointerNonNull();
        }
        GCNode* getGCNodeOrString() const {
            return static_cast<GCNode*>(m_value.getEitherPointer());
        }

        //@ Store a single integer.
        void setInteger(int value) {
            m_value.setInteger(IntegerTag, value);
        }
        bool isInteger() const {
            return m_value.isInteger(IntegerTag);
        }

        // These functions return references so that callers can get a pointer
        // to the value if desired.
        const int& getInteger() const {
            return m_value.getInteger(IntegerTag);
        }
        int& getInteger() {
            return m_value.getInteger(IntegerTag);
        }

        //@ Store a single Logical.
        void setLogical(Logical value) {
            m_value.setInteger(LogicalTag, static_cast<int>(value));
        }
        bool isLogical() const {
            return m_value.isInteger(LogicalTag);
        }

        const Logical& getLogical() const {
            static_assert(sizeof(Logical) == sizeof(int),
                          "Logical has unexpected fields");
            return reinterpret_cast<const Logical&>(
                m_value.getInteger(LogicalTag));
        }
        Logical& getLogical() {
            return reinterpret_cast<Logical&>(m_value.getInteger(LogicalTag));
        }

        //@ store a single double.  Note that not all doubles can be stored.
        void setDouble(double value) {
            m_value.setDouble(value);
        }
        bool isDouble() const {
            return m_value.isDouble();
        }
        const double& getDouble() const {
            return m_value.getDouble();
        }
        // No function returning a double lvalue, as not all values are legal.

        //* A value is storable if we can store and retrieve it.
        static bool isStorableDoubleValue(double d) {
            return DiscriminatedUnion::isStorableDoubleValue(d);
        }

        //@ Store a null value, separate from the GCNode and String types.
        // void setNull() { m_value.setNull(); }
        // bool isNullValue() const { return m_value.isNullValue(); }

        //@ GC related functions.
        void detachReferents() {
            if (isNonNullGCNodeOrString())
                getGCNodeOrString()->detachReferents();
        }
        void visitReferents(GCNode::const_visitor* v) {
            if (isNonNullGCNodeOrString())
                getGCNodeOrString()->visitReferents(v);
        }
        void incRefCount() const noexcept {
            if (isNonNullGCNodeOrString())
                GCNode::incRefCount(getGCNodeOrString());
        }
        void decRefCount() {
            if (isNonNullGCNodeOrString())
                GCNode::decRefCount(getGCNodeOrString());
        }

        // If candidate_pointer might be an encoded pointer, return the pointer
        // that it might be.  Else return null.
        static void* interpret_possible_pointer(void* candidate_pointer) {
            return DiscriminatedUnion::interpret_possible_pointer(
                candidate_pointer);
        }

        friend class GCRootBase;
        friend class GCStackRootBase;
        template<typename T> friend class GCEdge;
        template<typename T> friend class GCStackRoot;
    private:
        DiscriminatedUnion m_value;

        static constexpr uint16_t IntegerTag = 0;
        static constexpr uint16_t LogicalTag = 1;
    };

    template<typename T>
    class GCValue : public GCValue<GCNode>
    {
    public:
        /* implicit */ GCValue(const T* value) : GCValue<GCNode>(value) {}

        GCValue(const GCValue<T>&) = default;

        /* implicit */ operator T*() const {
            return static_cast<T*>(
                static_cast<GCNode*>(
                    static_cast<const GCValue<GCNode>&>(*this)));
        }

        T& operator*() const {
            const T* pointer = *this;
            return *pointer;
        }
    };
}  // namespace rho

#ifdef HAVE_GC_HEADER
#include "gc.h"

inline void rho::internal::ensureReachable(const void* p) {
    GC_reachable_here(p);
}

#endif  // HAVE_GC_HEADER

#endif  // RHO_GCVALUE_HPP
