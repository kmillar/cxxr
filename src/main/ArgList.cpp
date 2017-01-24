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
 *  https://www.R-project.org/Licenses/
 */

/** @file ArgList.cpp
 *
 * Implementation of class ArgList.
 */

#include "rho/ArgList.hpp"

#include <list>
#include "rho/DottedArgs.hpp"
#include "rho/Environment.hpp"
#include "rho/Evaluator.hpp"
#include "rho/Expression.hpp"
#include "rho/Promise.hpp"
#include "rho/errors.hpp"

using namespace std;
using namespace rho;

// Implementation of ArgList::coerceTag() is in coerce.cpp

static DottedArgs* getDottedArgs(Environment* env)
{
    Frame::Binding* binding = env->findBinding(DotsSymbol);
    if (!binding) {
	Rf_error(_("'...' used in an incorrect context"));
    }
    RObject* dots = binding->forcedValue();
    if (!dots || dots == Symbol::missingArgument()) {
	return nullptr;
    }
    if (dots->sexptype() != DOTSXP) {
	Rf_error(_("'...' used in an incorrect context"));
    }
    return static_cast<DottedArgs*>(dots);
}

ArgList::ArgList(const PairList* args, Status status)
    : m_status(status)
{
    for (const auto& arg : *args) {
	emplace_back(SEXP_downcast<const Symbol*>(arg.tag()), arg.car());
    }
}

ArgList::ArgList(std::initializer_list<RObject*> args, Status status)
    : m_status(status) {
    for (RObject* arg : args) {
	emplace_back(nullptr, arg);
    }
}

template<typename Function>
void ArgList::transform(Environment* env, Function fun)
{
    if (m_status != RAW || !has3Dots()) {
	// We can do this in-place.
	for (int i = 0; i < size(); i++) {
	    fun((*this)[i], i + 1);
	}
    } else {
	// We may need to expand out '...'.
	Vector<Argument> transformed;
	int arg_number = 1;
	for (Argument& arg : *this) {
	    if (arg.value() == DotsSymbol) {
		DottedArgs* dot_args = getDottedArgs(env);
		if (dot_args) {
		    for (const auto& dot_arg : *dot_args) {
			transformed.push_back(
			    Argument(SEXP_downcast<const Symbol*>(dot_arg.tag()),
				     dot_arg.car()));
			fun(transformed.back(), arg_number);
			arg_number++;
		    }
		}
	    } else {
		transformed.push_back(std::move(arg));
		fun(transformed.back(), arg_number);
		arg_number++;
	    }
	}
	static_cast<Vector<Argument>&>(*this) = std::move(transformed);
    }
}

void ArgList::evaluateToArray(Environment* env,
			      int num_args, RObject** evaluated_args,
			      MissingArgHandling allow_missing) const
{
    assert(allow_missing != MissingArgHandling::Drop);
    unsigned int arg_number = 0;
    Status status = m_status;

    if (status == EVALUATED) {
	for (const Argument& arg : *this) {
	    evaluated_args[arg_number] = arg.value();
	    ++arg_number;
	}
	assert(arg_number == num_args);
	return;
    }

    for (const Argument& arg : *this) {
	if (status == RAW && arg.value() == DotsSymbol) {
	    DottedArgs* dot_args = getDottedArgs(env);
	    if (dot_args) {
		for (const auto& dot_arg : *dot_args) {
		    evaluated_args[arg_number] =
			evaluateSingleArgument(dot_arg.car(), env,
					       allow_missing, arg_number + 1);
		    arg_number++;
		}
	    }
	} else {
	    evaluated_args[arg_number] =
		evaluateSingleArgument(arg.value(), env, allow_missing,
					   arg_number + 1);
	    arg_number++;
	}
    }
    assert(arg_number == num_args);
}

void ArgList::evaluate(Environment* env,
		       MissingArgHandling allow_missing)
{
    if (m_status == EVALUATED)
	return;

    transform(env,
	      [=](Argument& arg, int arg_number) {
		  arg.setValue(
		      evaluateSingleArgument(arg.value(), env, allow_missing,
					     arg_number));
	      });
    m_status = EVALUATED;
}

RObject* ArgList::evaluateSingleArgument(const RObject* arg,
					 Environment* env,
					 MissingArgHandling allow_missing,
					 int arg_number) const {
    if (arg && arg->sexptype() == SYMSXP) {
	const Symbol* sym = static_cast<const Symbol*>(arg);
	if (sym == Symbol::missingArgument()) {
	    if (allow_missing == MissingArgHandling::Keep)
		return Symbol::missingArgument();
	    else Rf_error(_("argument %d is empty"), arg_number);
	} else if (allow_missing == MissingArgHandling::Keep
		   && isMissingArgument(sym, env->frame())) {
	    return Symbol::missingArgument();
	    // allow_missing = false && isMissingArgument() is handled in
	    // evaluate() below.
	}
    }
    return Evaluator::evaluate(const_cast<RObject*>(arg), env);
}

const PairList* ArgList::list() const {
    PairList* result = nullptr;
    for (auto iter = rbegin(); iter != rend(); ++iter) {
	result = PairList::cons(iter->value(), result, iter->tag());
    }
    return result;
};

void ArgList::merge(const ConsCell* extraargs)
{
    if (m_status != PROMISED)
	Rf_error("Internal error: ArgList::merge() requires PROMISED ArgList");

    // Convert extraargs into a doubly linked list:
    typedef std::list<pair<const Symbol*, RObject*> > Xargs;
    Xargs xargs;
    for (const ConsCell* cc = extraargs; cc; cc = cc->tail())
	xargs.push_back(
	    make_pair(SEXP_downcast<const Symbol*>(cc->tag()), cc->car()));

    // Apply overriding arg values supplied in extraargs:
    for (Argument& arg : *this) {
	const RObject* tag = arg.tag();
	if (tag) {
	    Xargs::iterator it = xargs.begin();
	    while (it != xargs.end() && (*it).first != tag)
		++it;
	    if (it != xargs.end()) {
		arg.setValue(it->second);
		xargs.erase(it);
	    }
	}
    }

    // Append remaining extraargs:
    for (Xargs::const_iterator it = xargs.begin(); it != xargs.end(); ++it) {
	emplace_back(it->first, it->second);
    }
}

bool ArgList::has3Dots() const {
    for (auto& arg : *this) {
	if (arg.value() == R_DotsSymbol)
      return true;
  }
  return false;
}

bool ArgList::hasTags() const {
    for (auto& arg : *this) {
	if (arg.tag())
      return true;
  }
  return false;
}

void ArgList::stripTags()
{
    for (auto& item : *this) {
	item.setTag(nullptr);
    }
}

const Symbol* ArgList::tag2Symbol(const RObject* tag)
{
    return ((!tag || tag->sexptype() == SYMSXP)
	    ? static_cast<const Symbol*>(tag)
	    : coerceTag(tag));
}

void ArgList::wrapInPromises(Environment* env,
			     const Expression* call)
{
    if (m_status == PROMISED)
	return;
    if (m_status == EVALUATED)
    {
	// 'this' contains the evaluated values.  The expressions are in call.
	assert(call != nullptr);
	ArgList* values = this;
	ArgList raw_args(call->tail(), RAW);
	raw_args.transform(env, [=](Argument& arg, int arg_number) {
		if (arg_number - 1 > values->size()) {
		    Rf_error(_("dispatch error"));
		}
		arg.wrapInEvaluatedPromise((*values)[arg_number - 1].value());
	    });
	*this = std::move(raw_args);
	m_status = PROMISED;
	return;
    }
    assert(env != nullptr);

    transform(env, [=](Argument& arg, int arg_number) {
	    if (arg.value() != Symbol::missingArgument())
		arg.wrapInPromise(env);
	});
    m_status = PROMISED;
}
