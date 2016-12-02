/*
 *  R : A Computer Language for Statistical Data Analysis
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

#ifndef RHO_ARGUMENT_HPP
#define RHO_ARGUMENT_HPP

#include "rho/GCEdge.hpp"
#include "rho/GCNode.hpp"
#include "rho/Promise.hpp"
#include "rho/RObject.hpp"

namespace rho {

class Environment;
class Symbol;

/** @brief Class encapsulating an argument to a function.
 */
class Argument {
  public:
    Argument(const Symbol* tag, RObject* value)
        : m_tag(tag), m_is_promise_data(false), m_promise_data(nullptr) {
        m_value = value;
    }

    // NB: when copying Arguments that are wrapped in promises, the promise
    //  may be duplicated.  Calling code should ensure that only one gets
    //  evaluated.
    Argument(const Argument&) = default;
    Argument(Argument&&) = default;
    Argument& operator=(const Argument&) = default;
    Argument& operator=(Argument&&) = default;

    const Symbol* tag() const {
        return m_tag;
    }

    //@ brief Get the stored value.
    RObject* value() const {
        if (m_is_promise_data) {
            boxPromise();
        }
        return m_value;
    }

    /** @brief Get the value, evaluating promises.
     *
     * Returns the stored value if it is not a promise.  If it is a promise,
     * returns the evaluated value of the promise instead.
     */
    RObject* forcedValue() {
        if (m_is_promise_data) {
            return m_promise_data.evaluate();
        } else if (m_value->sexptype() == PROMSXP) {
            return static_cast<Promise*>(m_value.get())->force();
        } else {
            return m_value;
        }
    }

    void setValue(RObject* value) {
        m_value = value;
        if (m_is_promise_data) {
            m_promise_data.detachReferents();
            m_is_promise_data = false;
        }
    }

    void setTag(const Symbol* tag) {
        m_tag = tag;
    }

    /** @brief Convert this into a promise.
     *
     * @param env The environment to evaluate the promise in.
     *
     * @note This function requires that the value has not been already
     *    wrapped in a promise and also that the value is not '...'.
     */
    void wrapInPromise(Environment* env);

    /** @brief Convert this into an already-evaluated promise.
     *
     * @param value The value of the evaluated promise.
     *
     * @note This function requires that the value has not been already
     *    wrapped in a promise and also that the value is not '...'.
     */
    void wrapInEvaluatedPromise(RObject* value);

    void detachReferents();
    void visitReferents(GCNode::const_visitor* v) const;
  private:
    const Symbol* m_tag;
    mutable bool m_is_promise_data;
    // Only one of these fields will ever be set at any point in time.
    mutable GCEdge<> m_value;
    mutable PromiseData m_promise_data;

    void boxPromise() const;
};

}  // namespace rho

#endif  // RHO_ARGUMENT_HPP
