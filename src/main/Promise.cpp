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
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, a copy is available at
 *  https://www.R-project.org/Licenses/
 */

/** @file Promise.cpp
 *
 * @brief Implementation of class Promise and associated C
 * interface.
 */

#include "rho/Promise.hpp"

#include "localization.h"
#include "R_ext/Error.h"
#include "rho/Bailout.hpp"
#include "rho/Expression.hpp"
#include "rho/GCStackRoot.hpp"
#include "rho/PlainContext.hpp"
#include "rho/StackChecker.hpp"

using namespace rho;

// Force the creation of non-inline embodiments of functions callable
// from C:
namespace rho {
    namespace ForceNonInline {
	SEXP (*PRCODEp)(SEXP x) = PRCODE;
	SEXP (*PRENVp)(SEXP x) = PRENV;
	SEXP (*PRVALUEp)(SEXP x) = PRVALUE;
    }
}

PromiseData::PromiseData(const PromiseData& source)
    : PromiseData(source.asPromise())
{}

PromiseData::PromiseData(const RObject* valgen, Environment* env)
    : m_is_pointer_to_promise(false), m_under_evaluation(false),
      m_interrupted(false), m_is_evaluated(false)
{
    m_valgen = valgen;
    m_value = env;
}

PromiseData::PromiseData(Promise* value)
    : m_is_pointer_to_promise(true)
{
    m_value = value;
}

PromiseData& PromiseData::operator=(const PromiseData& source) {
    m_value = source.asPromise();
    m_is_pointer_to_promise = true;
    m_valgen = nullptr;
    return *this;
}

Promise* PromiseData::asPromise() const {
    if (m_is_pointer_to_promise) {
	return SEXP_downcast<Promise*>(m_value.get());
    }
    Promise* promise = new Promise(std::move(*const_cast<PromiseData*>(this)));
    *const_cast<PromiseData*>(this) = PromiseData(promise);
    return promise;
}

void PromiseData::detachReferents()
{
    m_value.detach();
    m_valgen.detach();
}

RObject* PromiseData::evaluate()
{
    if (m_is_pointer_to_promise) {
	return getThis()->evaluate();
    }
    if (!m_is_evaluated || m_value == Symbol::unboundValue()) {
	// Force promise:
	if (m_interrupted) {
	    Rf_warning(_("restarting interrupted promise evaluation"));
	    m_interrupted = false;
	}
	else if (m_under_evaluation)
	    Rf_error(_("promise already under evaluation: "
		       "recursive default argument reference "
		       "or earlier problems?"));
	m_under_evaluation = true;
	try {
	    IncrementStackDepthScope scope;
	    PlainContext cntxt;
	    RObject* val = Evaluator::evaluate(
		const_cast<RObject*>(m_valgen.get()),
		getEnvironment());
	    setEvaluatedValue(val);
	}
	catch (...) {
	    m_interrupted = true;
	    throw;
	}
	m_under_evaluation = false;
    }
    assert(!m_is_pointer_to_promise);
    return getValue();
}

bool PromiseData::isMissingSymbol() const
{
    if (m_is_pointer_to_promise) {
	return getThis()->isMissingSymbol();
    }

    bool ans = false;
    /* This is wrong but I'm not clear why - arr
    if (m_value == Symbol::missingArgument())
	return true;
    */
    if ((!m_is_evaluated || m_value == Symbol::unboundValue()) && m_valgen) {
	const RObject* prexpr = m_valgen;
	if (prexpr->sexptype() == SYMSXP) {
	    // According to Luke Tierney's comment to R_isMissing() in CR,
	    // if a cycle is found then a missing argument has been
	    // encountered, so the return value is true.
	    if (m_under_evaluation)
		return true;
	    try {
		const Symbol* promsym
		    = static_cast<const Symbol*>(prexpr);
		m_under_evaluation = true;
		ans = isMissingArgument(promsym, getEnvironment()->frame());
	    }
	    catch (...) {
		m_under_evaluation = false;
		throw;
	    }
	    m_under_evaluation = false;
	}
    }
    assert(!m_is_pointer_to_promise);
    return ans;
}

void PromiseData::setEvaluatedValue(RObject* val)
{
    assert(!m_is_pointer_to_promise);
    if (val == Symbol::unboundValue()) {
	return;
    }
    m_value = val;
    m_is_evaluated = true;
    SET_NAMED(val, 2);
}

Environment* PromiseData::getEnvironment() const {
    assert(!m_is_pointer_to_promise);
    if (m_is_evaluated)
	return nullptr;
    return SEXP_downcast<Environment*>(m_value.get());
}

const char* Promise::typeName() const
{
    return staticTypeName();
}

Environment* Promise::environment_full() const {
    return m_data.getEnvironment();
}

void PromiseData::visitReferents(GCNode::const_visitor* v) const
{
    const GCNode* value = m_value;
    const GCNode* valgen = m_valgen;
    if (value)
	(*v)(value);
    if (valgen)
	(*v)(valgen);
}

// ***** C interface *****

SEXP Rf_mkPROMISE(SEXP expr, SEXP rho)
{
    GCStackRoot<> exprt(expr);
    GCStackRoot<Environment> rhort(SEXP_downcast<Environment*>(rho));
    return new Promise(exprt, rhort);
}

SEXP R_mkEVPROMISE(SEXP expr, SEXP value)
{
    return Promise::createEvaluatedPromise(
	SEXP_downcast<Expression*>(expr), value);
}

void SET_PRVALUE(SEXP x, SEXP v)
{
    Promise* prom = SEXP_downcast<Promise*>(x);
    prom->setEvaluatedValue(v);
}

int PRSEEN(SEXP x) {
    Promise* prom = SEXP_downcast<Promise*>(x);
    return prom->m_data.m_under_evaluation
	|| prom->m_data.m_interrupted
	|| prom->m_data.m_is_evaluated;
}

SEXP PRENV(SEXP x) {
    const Promise& prom = *SEXP_downcast<Promise*>(x);
    return prom.environment();
}
