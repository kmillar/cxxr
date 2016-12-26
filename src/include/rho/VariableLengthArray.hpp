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

#ifndef RHO_GC_BUFFER_HPP
#define RHO_GC_BUFFER_HPP

#include "rho/GCNode.hpp"
#include "rho/MemoryBank.hpp"
#include <iterator>
#include <memory>

namespace rho {

/** @brief GC-aware data storage.
 *
 * VariableLengthArray is a GC-aware array, with the ability to change size,
 * up to the capacity that was allocated when the object was created.
 *
 * It is a building block for rho::Vector.
 */
template<typename T>
class VariableLengthArray : public GCNode {
  public:
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T* iterator;
    typedef const T* const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef std::ptrdiff_t difference_type;
    typedef size_t size_type;

    static VariableLengthArray* create(size_type capacity) {
        void* storage = GCNode::operator new(sizeof(VariableLengthArray<T>)
                                             + capacity * sizeof(T));
        return new(storage) VariableLengthArray(capacity);
    }

    // No copy or move operators.
    VariableLengthArray(const VariableLengthArray&) = delete;
    VariableLengthArray(VariableLengthArray&&) = delete;
    VariableLengthArray& operator=(const VariableLengthArray&) = delete;
    VariableLengthArray& operator=(VariableLengthArray&&) = delete;

    iterator begin() noexcept              {
        return reinterpret_cast<iterator>(reinterpret_cast<void*>(this + 1)); }
    const_iterator begin() const noexcept  {
        return const_cast<VariableLengthArray<T>*>(this)->begin(); }
    const_iterator cbegin() const noexcept { return begin(); }
    reverse_iterator rbegin() noexcept     { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return crbegin(); }
    const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(end()); }

    iterator end() noexcept              { return begin() + size(); }
    const_iterator end() const noexcept  { return begin() + size(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rend() noexcept     { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return crend(); }
    const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(begin()); }

    size_type size() const noexcept      { return m_size; }
    size_type max_size() const noexcept  { return capacity(); }
    size_type capacity() const noexcept  { return m_capacity; }
    bool empty() const noexcept          { return size() == 0; }
    // reserve() and shrink_to_fit() aren't implemented.

    void resize(size_type count) {
        assert(count <= capacity());
        if (count <= size()) {
            shrinkToSize(count);
        } else {
            for (size_type i = 0; i < count; ++i) {
                // NB: uses default-initialization, which is a no-op for fundamental
                //   types.
                new (end() + i)value_type;
            }
            m_size = count;
        }
    }

    void resize(size_type count, const value_type& value) {
        assert(count <= capacity());
        if (count <= size()) {
            destroy_last_n_elements(size() - count);
        } else {
            std::uninitialized_fill(end(), begin() + count, value);
            m_size = count;
        }
    }

    T& operator[](size_type t) noexcept {
        assert(t < size());
        return *(begin() + t);
    }
    const T& operator[](size_type t) const noexcept {
        assert(t < size());
        return *(begin() + t);
    }
    // at() not implemented.

    reference front() noexcept              { return *begin(); }
    const_reference front() const noexcept  { return *begin(); }
    reference back() noexcept               { return *rbegin(); }
    const_reference back() const noexcept   { return *rbegin(); }

    T* data() noexcept              { return begin(); }
    const T* data() const noexcept  { return begin(); }

    template<typename InputIt>
    void assign(InputIt first, InputIt last) {
        size_type n = last - first;
        assert(n <= capacity());
        if (n < size()) {
            std::copy(first, last, begin());
            shrinkToSize(n);
        } else {
            size_type m = size();
            // Elements [first, first + m) get copied to initialized memory
            // at [begin, end)
            std::copy(first, first + m, begin());
            // Elements [first + m, last) get copied to uninitialized memory
            // at [end, begin + n)
            std::uninitialized_copy(first + m, last, end());
            m_size = n;
        }
    }

    void assign(size_type n, const T& value) {
        assert(n <= capacity());
        if (size() > n) {
            shrinkToSize(n);
            std::fill(begin(), end(), value);
        } else {
            std::fill(begin(), end(), value);
            std::uninitialized_fill(end(), begin() + n, value);
            m_size = n;
        }
    }
    void assign(std::initializer_list<T> values) {
        assign(values.begin(), values.end()); }

    void push_back(const T& item) { emplace_back(item); }
    void push_back(T&& item)      { emplace_back(std::forward<T>(item)); }
    void pop_back() noexcept      { destroy_last_n_elements(1); }

    void insert(const_iterator pos, const T& value) {
        assert(size() + 1 <= capacity());
        if (pos == end()) {
            push_back(value);
        } else {
            moveElementsForward(pos, end(), pos + 1);
            *pos = value;
            m_size++;
        }
    }
    iterator insert(const_iterator pos, T&& value) {
        assert(size() + 1 <= capacity());
        if (pos == end()) {
            push_back(std::forward<T>(value));
        } else {
            moveElementsForward(pos, end(), pos + 1);
            *pos = std::forward<T>(value);
            m_size++;
        }
    }

    template<typename InputIt>
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        size_type n = last - first;
        assert(size() + n <= capacity());

        // Move elements after pos forward by n elements.
        moveElementsForward(pos, end(), pos + n);

        // Insert new elements.
        if (pos + n < end()) {
            std::copy(first, last, pos);
        } else {
            iterator q = first + (end() - pos);
            // Elements in [first, q) get copied to [pos, end).
            std::copy(first, q, pos);
            // Elements in [q, last) get copied to [end, pos + n)
            std::uninitialized_copy(q, last, end());
        }
        m_size += n;
    }

    template<typename InputIt>
    iterator insert(const_iterator pos, std::initializer_list<T> values) {
        return insert(pos, values.begin(), values.end());
    }

    iterator erase(const_iterator pos) noexcept {
        return erase(end() - 1);
    }
    iterator erase(const_iterator first, const_iterator last) noexcept {
        std::move(last, end(), first);
        destroy_last_n_elements(last - first);
    }

    // swap() not implemented.
    void clear() noexcept { shrinkToSize(0); }

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        assert(size() + 1 < capacity());
        moveElementsForward(pos, end(), pos + 1);
        *pos = value_type(std::forward<Args>(args)...);
        m_size++;
    }

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        assert(size() + 1 < capacity());
        new (end()) value_type(std::forward<Args>(args)...);
        m_size++;
        return back();
    }

    // GCNode methods.
    void detachReferents() override { clear(); }
    void visitReferents(const_visitor* v) const override {
        for (const value_type& element : *this) {
            visitObjectOrReferents(element, v);
        }
        GCNode::visitReferents(v);
    }

  protected:
    ~VariableLengthArray()
    {
        // GCNode::~GCNode doesn't know about the storage space in
        // this object, so account for it here.
        size_t bytes = (capacity()) * sizeof(T);
        if (bytes != 0) {
            MemoryBank::adjustFreedSize(sizeof(VariableLengthArray),
                                        sizeof(VariableLengthArray) + bytes);
        }

        clear();
    }
  private:
    template<typename, unsigned N>
    friend class Vector;

    size_type m_size;
    size_type m_capacity;
    // Data follows *this.

    VariableLengthArray(size_type capacity) {
        m_size = 0;
        m_capacity = capacity;
    }

    static void uninitialized_move(iterator first, iterator last,
                                   iterator dest) {
        std::move(first, last,
                  std::raw_storage_iterator<iterator, value_type>(dest));
    }

    void shrinkToSize(size_type count) noexcept {
        destroy_last_n_elements(size() - count);
    }

    void destroy_last_n_elements(size_type n) noexcept {
        assert(n <= size());
        for (iterator pos = end() - n; pos != end(); ++pos) {
            pos->~value_type();
        }
        m_size -= n;
    }

    void moveElementsForward(iterator first, iterator last, iterator dest) {
        size_t n = last - first;
        if (dest + n < end()) {
            std::move_backward(first, last, dest + n);
        } else {
            iterator p = end() - n;
            // Elements in [p, last) get moved to the uninitialized memory at
            // [end(), end() + n)
            uninitialized_move(p, last, end());
            // Elements in [first, p) get moved to the initialized memory at
            // [dest, end())
            std::move_backward(first, p, end());
        }
    }
};

}  // namespace rho

#endif  // RHO_GC_BUFFER_HPP
