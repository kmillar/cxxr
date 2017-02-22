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

#ifndef RHO_VECTOR_HPP
#define RHO_VECTOR_HPP

#include "rho/GCNode.hpp"
#include "rho/GCEdge.hpp"
#include "rho/VariableLengthArray.hpp"
#include <iterator>
#include <memory>
#include <stdexcept>

namespace rho {

/** @brief GC-aware version of std::vector with small size optimization.
 *
 * Vector implements the API of std::vector, except that moving a Vector invalidates
 * all iterators and Vector(size_type) and resize() don't initialize fundamental
 * types to zero.
 *
 * Unlinke std::vector, Vector is integrated with rho's memory management.  It can
 * be stored in a GCEdge and if it contains GCEdge objects, they will be correctly
 * protected from GC.
 *
 * \tparam T The type to be stored in the Vector.
 * \tparam N The number of elements to store locally in the vector.  The Vector does
 *    not allocate memory unless the required capacity exceeds N.
 */
template<typename T, unsigned N = 4>
class Vector {
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
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;

    Vector() {
        m_size = 0;
        m_is_small = true;
    }
    explicit Vector(size_type count, const value_type& value) : Vector() {
        assign(count, value);
    }
    explicit Vector(size_type count) : Vector() {
        resize(count);
    }
    template<typename ForwardIterator>
    Vector(ForwardIterator first, ForwardIterator last) : Vector() {
        size_type n = last - first;
        reallocateIfNeeded(n);
        std::uninitialized_copy(first, last, begin());
        setSize(n);
    }

    Vector(std::initializer_list<value_type> values) : Vector() {
        assign(values);
    }

    ~Vector() {
        detachReferents();
    }

    Vector(const Vector& other) : Vector(other.begin(), other.end()) {}
    Vector& operator=(const Vector&other ) { assign(other.begin(), other.end()); }

    // This assumes POD data.  That seems broken.
    Vector(Vector&& other)
        : m_data(other.m_data), m_size(other.m_size),
          m_is_small(other.m_is_small)
    {
        other.m_size = 0;
        other.m_is_small = true;
    }
    Vector& operator=(Vector&& other) {
        this->~Vector();
        new (this)Vector(std::move(other));
        return *this;
    }

    iterator begin() noexcept              { return isSmall()
            ? reinterpret_cast<iterator>(m_data.m_storage) : getPointer()->begin(); }
    const_iterator begin() const noexcept  { return isSmall()
            ? reinterpret_cast<const_iterator>(m_data.m_storage)
            : getPointer()->begin(); }
    const_iterator cbegin() const noexcept { return begin(); }
    reverse_iterator rbegin() noexcept     { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept {
        return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(end()); }

    iterator end() noexcept              { return begin() + size(); }
    const_iterator end() const noexcept  { return begin() + size(); }
    const_iterator cend() const noexcept { return end(); }
    reverse_iterator rend() noexcept     { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept {
        return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(begin()); }

    size_type size() const noexcept      { return m_size; }
    size_type max_size() const noexcept  {
        return (1L << 48) - 1; }
    size_type capacity() const noexcept  {
        return isSmall() ? N : getPointer()->capacity(); }
    bool empty() const noexcept          { return size() == 0; }
    void reserve(size_type size)         { reallocateIfNeeded(size); }
    void shrink_to_fit()                 { /* unimplemented for now */ }

    void resize(size_type count) {
        if (count <= size()) {
            shrinkToSize(count);
        } else {
            reallocateIfNeeded(count);
            for (size_type i = 0; i < count; ++i) {
                // NB: uses default-initialization, which is a no-op for fundamental
                //   types.
                new (end() + i)value_type;
            }
            setSize(count);
        }
    }

    void resize(size_type count, const value_type& value) {
        if (count <= size()) {
            shrinkToSize(count);
        } else {
            reallocateIfNeeded(count);
            std::uninitialized_fill(end(), begin() + count, value);
            setSize(count);
        }
    }

    T& operator[](size_type t) noexcept             {
        assert(t < size());
        return *(begin() + t);
    }
    const T& operator[](size_type t) const noexcept {
        assert(t < size());
        return *(begin() + t);
    }
    T& at(size_type t) {
        if (t < size()) throw std::out_of_range("Vector");
        return (*this)[t];
    }
    const T& at(size_type t) const {
        if (t < size()) throw std::out_of_range("Vector");
        return (*this)[t];
    }

    reference front() noexcept              { return *begin(); }
    const_reference front() const noexcept  { return *begin(); }
    reference back() noexcept               { return *rbegin(); }
    const_reference back() const noexcept   { return *rbegin(); }

    T* data() noexcept              { return begin(); }
    const T* data() const noexcept  { return begin(); }

    template<typename InputIt,
             typename = typename std::enable_if<
                 std::is_base_of<std::input_iterator_tag,
                                 typename std::iterator_traits<InputIt>::iterator_category>
                 ::value>::type>
    void assign(InputIt first, InputIt last) {
        assert(last >= first);
        size_type n = last - first;
        if (n < size()) {
            std::copy(first, last, begin());
            shrinkToSize(n);
        }
        else {
            size_type m = size();
            reallocateIfNeeded(n);
            // Elements [first, first + m) get copied to initialized memory
            // at [begin, end)
            std::copy(first, first + m, begin());
            // Elements [first + m, last) get copied to uninitialized memory
            // at [end, begin + n)
            std::uninitialized_copy(first + m, last, end());
            setSize(n);
        }
    }

    void assign(size_type n, const T& value) {
        if (size() > n) {
            shrinkToSize(n);
            std::fill(begin(), begin() + n, value);
        }
        else {
            reallocateIfNeeded(n);
            std::fill(begin(), end(), value);
            std::uninitialized_fill(end(), begin() + n, value);
            setSize(n);
        }
    }

    void assign(std::initializer_list<T> values) {
        assign(values.begin(), values.end()); }

    void push_back(const T& item) { emplace_back(item); }
    void push_back(T&& item)      { emplace_back(std::forward<T>(item)); }
    void pop_back() noexcept      { destroy_last_n_elements(1); }

    iterator insert(const_iterator pos, const T& value) {
        if (pos == end()) {
            push_back(value);
            return &back();
        } else {
            size_type offset = pos - begin();
            reallocateIfNeeded(size() + 1);
            iterator fixed_pos = begin() + offset;

            moveElementsForward(fixed_pos, end(), fixed_pos + 1);
            *fixed_pos = value;
            setSize(size() + 1);
            return fixed_pos;
        }
    }
    iterator insert(const_iterator pos, T&& value) {
        if (pos == end()) {
            push_back(std::forward<T>(value));
            return &back();
        } else {
            size_type offset = pos - begin();
            reallocateIfNeeded(size() + 1);
            iterator fixed_pos = begin() + offset;

            moveElementsForward(fixed_pos, end(), fixed_pos + 1);
            *fixed_pos = std::forward<T>(value);
            setSize(size() + 1);
            return fixed_pos;
        }
    }

    template<typename InputIt,
             typename = typename std::enable_if<
                 std::is_base_of<std::input_iterator_tag,
                                 typename std::iterator_traits<InputIt>::iterator_category>
                 ::value>::type>
    void insert(const_iterator pos_, InputIt first, InputIt last) {
        if (first == last) {
            return;
        }
        size_type n = last - first;
        size_type offset = pos_ - begin();
        reallocateIfNeeded(size() + n);
        iterator pos = begin() + offset;

        // Move elements after pos forward by n elements.
        moveElementsForward(pos, end(), pos + n);

        // Insert new elements.
        if (pos + n < end()) {
            std::copy(first, last, pos);
        } else {
            InputIt q = first + (end() - pos);
            // Elements in [first, q) get copied to [pos, end).
            std::copy(first, q, const_cast<iterator>(pos));
            // Elements in [q, last) get copied to [end, pos + n)
            std::uninitialized_copy(q, last, end());
        }
        setSize(size() + n);
    }

    template<typename InputIt>
    iterator insert(const_iterator pos, std::initializer_list<T> values) {
        return insert(pos, values.begin(), values.end());
    }

    iterator erase(const_iterator pos) noexcept {
        // Need to get rid of the 'const', so std::move can be used.
        iterator iter = const_cast<iterator>(pos);
        std::move(std::next(iter), end(), iter);
        destroy_last_n_elements(1);
        return iter;
    }
    iterator erase(const_iterator first, const_iterator last) noexcept {
        std::move(last, cend(), const_cast<iterator>(first));
        destroy_last_n_elements(last - first);
        return const_cast<iterator>(first);
    }

    void clear() noexcept {
        shrinkToSize(0);
    }

    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        if (pos == end()) {
            emplace_back(std::forward<Args>(args)...);
            return end() - 1;
        }
        size_type offset = pos - begin();
        reallocateIfNeeded(size() + 1);
        pos = begin() + offset;

        moveElementsForward(pos, end(), pos + 1);
        *pos = value_type(std::forward<Args>(args)...);
        setSize(size() + 1);
        return pos;
    }

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        reallocateIfNeeded(size() + 1);
        new (end()) value_type(std::forward<Args>(args)...);
        setSize(size() + 1);
        return back();
    }

    void swap(Vector& other) {
        Vector tmp(std::move(other));
        other = std::move(*this);
        (*this) = std::move(tmp);
    }

    // GCNode methods.  Classes that include a Vector object as a member need
    // to call these.
    void detachReferents() {
        if (isSmall()) {
            clear();
        } else {
            freeData();
        }
    }
    void visitReferents(GCNode::const_visitor* v) const {
        if (isSmall()) {
            for (const value_type& element : *this) {
                visitObjectOrReferents(element, v);
            }
        } else {
            (*v)(getPointer());
        }
    }

  private:
    typedef GCEdge<VariableLengthArray<T>> PointerType;

    // We use a small-size optimization.  For small vectors, we store the data
    // directly in this object.
    // Note that as a result, move() invalidates iterators, which std::vector
    // doesn't do.
    union {
        alignas(PointerType) char m_pointer[sizeof(PointerType)];
        static_assert(sizeof(value_type) % alignof(value_type) == 0,
                      "FIXME: sizeof(value_type) not a multiple of alignof(value_type) isn't supported yet.");
        alignas(value_type) char m_storage[sizeof(value_type) * N];
    } m_data;

    size_t m_size : 48;
    bool m_is_small : 1;

    void setSize(size_type size) noexcept {
        m_size = size;
        if (!isSmall()) {
            getPointer()->m_size = size;
        }
    }

    PointerType& getPointer() noexcept {
        assert(!isSmall());
        char* p = m_data.m_pointer;
        return *reinterpret_cast<PointerType*>(p);
    }
    const PointerType& getPointer() const noexcept {
        assert(!isSmall());
        const char* p = m_data.m_pointer;
        return *reinterpret_cast<const PointerType*>(p);
    }
    void setPointer(VariableLengthArray<T>* data) {
        assert(m_size == data->size());
        if (isSmall()) {
            // deallocate the existing objects.
            clear();
            // default-construct the pointer.
            m_is_small = false;
            PointerType* p = &getPointer();
            new (p) PointerType;
            m_size = data->size();
        }
        getPointer() = data;
    }

    void freeData() {
        if (!isSmall()) {
            getPointer().~PointerType();
            m_size = 0;
            m_is_small = true;
        }
    }

    bool isSmall() const noexcept {
        return m_is_small;
    }

    void reallocateIfNeeded(size_t new_capacity) {
        if (new_capacity <= capacity())
            return;
        reallocate(new_capacity);
    }

    void reallocate(size_t new_capacity) {
        // Ensure that capacity grows by at least 50%.
        new_capacity = std::max(new_capacity,capacity() + capacity() / 2);
        VariableLengthArray<T>* new_data
            = VariableLengthArray<T>::create(new_capacity);
        new_data->m_size = size();
        uninitialized_move(begin(), end(), new_data->begin());
        setPointer(new_data);
    }

    void shrinkToSize(size_type count) noexcept {
        destroy_last_n_elements(size() - count);
    }

    void destroy_last_n_elements(size_t n) {
        assert(size() >= n);
        iterator end = this->end();
        for (iterator pos = end - n; pos != end; ++pos) {
            pos->~value_type();
        }
        setSize(size() - n);
    };

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

    static void uninitialized_move(iterator first, iterator last,
                                   iterator dest) {
        VariableLengthArray<T>::uninitialized_move(first, last, dest);
    }
};

}  // namespace rho

#endif
