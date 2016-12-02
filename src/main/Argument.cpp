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

#include "rho/Argument.hpp"

using namespace rho;

void Argument::boxPromise() const {
    assert(m_is_promise_data);

    m_value = m_promise_data.asPromise();
    m_promise_data.detachReferents();
    m_is_promise_data = false;
}

void Argument::wrapInPromise(Environment* env) {
    assert(!m_is_promise_data);
    m_promise_data = PromiseData(m_value, env);
    m_is_promise_data = true;
    m_value = nullptr;
}

void Argument::wrapInEvaluatedPromise(RObject* value) {
    assert(!m_is_promise_data);
    m_promise_data = PromiseData::createEvaluatedPromise(m_value, value);
    m_is_promise_data = true;
    m_value = nullptr;
}

void Argument::visitReferents(GCNode::const_visitor* v) const {
    if (m_tag)
        (*v)(m_tag);
    if (m_is_promise_data) {
        m_promise_data.visitReferents(v);
    } else if (m_value) {
        (*v)(m_value);
    }
}

void Argument::detachReferents() {
    m_tag = nullptr;
    if (m_is_promise_data) {
        m_promise_data.detachReferents();
    } else {
        m_value = nullptr;
    }
}
