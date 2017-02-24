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

/** @file Expression.cpp
 *
 * @brief Class Expression and associated C interface.
 */
#define R_NO_REMAP

#include "rho/Expression.hpp"

#include <iostream>
#include <boost/preprocessor.hpp>

#include "R_ext/Error.h"
#include "localization.h"
#include "rho/ArgList.hpp"
#include "rho/ArgMatcher.hpp"
#include "rho/BuiltInFunction.hpp"
#include "rho/Closure.hpp"
#include "rho/ClosureContext.hpp"
#include "rho/Environment.hpp"
#include "rho/Evaluator.hpp"
#include "rho/FunctionContext.hpp"
#include "rho/FunctionBase.hpp"
#include "rho/GCStackFrameBoundary.hpp"
#include "rho/GCStackRoot.hpp"
#include "rho/PlainContext.hpp"
#include "rho/ProtectStack.hpp"
#include "rho/RAllocStack.hpp"
#include "rho/StackChecker.hpp"
#include "rho/Symbol.hpp"

#undef match

using namespace std;
using namespace rho;

// Force the creation of non-inline embodiments of functions callable
// from C:
namespace rho {
    namespace ForceNonInline {
	SEXP (*lconsp)(SEXP cr, SEXP tl) = Rf_lcons;
   }
}

GCRoot<> R_CurrentExpr;

Expression::Expression(RObject* function, std::initializer_list<RObject*> args)
    : Expression(function)
{
    ConsCell* current = this;
    for (RObject* arg : args) {
	PairList* next = new PairList(arg);
	current->setTail(next);
	current = next;
    }
}

Expression* Expression::clone() const
{
    return new CachingExpression(*this);
}

FunctionBase* Expression::getFunction(Environment* env) const
{
    RObject* head = car();
    if (head->sexptype() == SYMSXP) {
	Symbol* symbol = static_cast<Symbol*>(head);
	FunctionBase* func = findFunction(symbol, env);
	if (!func)
	    Rf_error(_("could not find function \"%s\""),
		  symbol->name()->c_str());
	return func;
    } else {
	RObject* val = Evaluator::evaluate(head, env);
	if (!FunctionBase::isA(val))
	    Rf_error(_("attempt to apply non-function"));
	return static_cast<FunctionBase*>(val);
    }
}

RObject* Expression::evaluate(Environment* env)
{
    FunctionBase* function = getFunction(env);
    return evaluateFunctionCall(function, env, getArgs(), ArgList::RAW);
}

RObject* Expression::evaluateFunctionCall(const FunctionBase* func,
                                          Environment* env,
					  const ArgList& args,
					  const Frame* method_bindings) const
{
    if (func->sexptype() != CLOSXP) {
	assert(method_bindings == nullptr);
	return evaluateFunctionCall(func, env, args.list(), args.status());
    }

    IncrementStackDepthScope scope;
    RAllocStack::Scope ras_scope;
    ProtectStack::Scope ps_scope;

    func->maybeTrace(this);

    auto closure = static_cast<const Closure*>(func);
    return GCStackFrameBoundary::withStackFrameBoundary(
	[=]() { return invokeClosure(closure, env, args, method_bindings); });
}

RObject* Expression::evaluateFunctionCall(const FunctionBase* func,
					  Environment* env,
					  const PairList* args,
					  ArgList::Status status) const {
    if (func->sexptype() == CLOSXP) {
	return evaluateFunctionCall(func, env, ArgList(args, status));
    }

    IncrementStackDepthScope scope;
    RAllocStack::Scope ras_scope;
    ProtectStack::Scope ps_scope;
    RObject* result;

    func->maybeTrace(this);
    assert(func->sexptype() == SPECIALSXP || func->sexptype() == BUILTINSXP);
    auto builtin = static_cast<const BuiltInFunction*>(func);

    if (builtin->createsStackFrame()) {
	FunctionContext context(this, env, builtin);
	result = evaluateBuiltInCall(builtin, env, args, status);
    } else {
	PlainContext context;
	result = evaluateBuiltInCall(builtin, env, args, status);
    }

    if (builtin->printHandling() != BuiltInFunction::SOFT_ON) {
	Evaluator::enableResultPrinting(
            builtin->printHandling() != BuiltInFunction::FORCE_OFF);
    }
    return result;
}

static void prepareToInvokeBuiltIn(const BuiltInFunction* func)
{
    if (func->printHandling() == BuiltInFunction::SOFT_ON) {
	Evaluator::enableResultPrinting(true);
    }

#ifdef Win32
    // This is an inlined version of Rwin_fpreset (src/gnuwin/extra.c)
    // and resets the precision, rounding and exception modes of a
    // ix86 fpu.
    // It gets called prior to every builtin function, just in case a badly
    // behaved DLL has changed the fpu control word.
    __asm__ ( "fninit" );
#endif
}

static bool hasDots(const PairList* args) {
    for (; args; args = args->tail()) {
	if (args->car() == R_DotsSymbol)
	    return true;
    }
    return false;
}

RObject* Expression::evaluateBuiltInCall(const BuiltInFunction* builtin,
					 Environment* env,
					 const PairList* args,
					 ArgList::Status status) const {
    bool needs_evaluation = builtin->sexptype() == BUILTINSXP
	&& status != ArgList::EVALUATED;

    // Take care of '...' if needed.
    if (needs_evaluation && hasDots(args))
    {
	ArgList expanded_args(args, status);
	expanded_args.evaluate(env);
	return evaluateBuiltInCall(builtin, env, expanded_args.list(),
				   expanded_args.status());
    }

    // Check the number of arguments.
    int num_args = listLength(args);
    builtin->checkNumArgs(num_args, this);

    // Check that any naming requirements on the first arg are satisfied.
    const char* first_arg_name = builtin->getFirstArgName();
    if (first_arg_name) {
	check1arg(first_arg_name);
    }

    if (builtin->hasFixedArityCall()) {
	return invokeFixedArityBuiltIn(builtin, env, args, num_args,
				       needs_evaluation);
    }

    if (builtin->hasDirectCall() || builtin->sexptype() == BUILTINSXP) {
	ArgList arglist(args, status);
	if (needs_evaluation)
	    arglist.evaluate(env);
	prepareToInvokeBuiltIn(builtin);
	return builtin->invoke(this, env, std::move(arglist));
    }

    assert(builtin->sexptype() == SPECIALSXP);
    prepareToInvokeBuiltIn(builtin);
    return builtin->invokeSpecial(this, env, args);
}

namespace {

// NB: This guarantees that the arguments are evaluated in order from left to
//     right, whereas the evaluation order in foo(evaluate(arg)...) is undefined.
template<unsigned N>
struct InvokeWithExpandedArgs {
    template<typename... ExpandedArgs>
    static RObject* invoke(const BuiltInFunction* func,
			   const Expression* call,
			   Environment* env,
			   const PairList* tags,
			   bool evaluate_args,
			   const PairList* unexpanded_args,
			   ExpandedArgs... expanded_args) {
	RObject* raw_arg = unexpanded_args->car();
	RObject* arg = (evaluate_args && raw_arg)
	    ? raw_arg->evaluate(env) : raw_arg;
	return InvokeWithExpandedArgs<N - 1>::invoke(
	    func, call, env, tags, evaluate_args,
	    unexpanded_args->tail(), expanded_args..., arg);
    }
};

template<>
struct InvokeWithExpandedArgs<0> {
    template<typename... ExpandedArgs>
    static RObject* invoke(const BuiltInFunction* func,
			   const Expression* call,
						      Environment* env,
			   const PairList* tags,
			   bool evaluate_args,
			   const PairList* unexpanded_args,
			   ExpandedArgs... expanded_args) {
	assert(unexpanded_args == nullptr);
    prepareToInvokeBuiltIn(func);
	return func->invokeFixedArity(call, env, tags, expanded_args...);
}
};

}

RObject* Expression::invokeFixedArityBuiltIn(const BuiltInFunction* func,
					       Environment* env,
					     const PairList* args, int num_args,
					     bool needs_evaluation) const
{
    const PairList* tags = args;

    switch (num_args) {
/*  This macro expands out to:
    case 0:
	return InvokeWithExpandedArgs<0>::invoke(func, this, env, tags,
						 needs_evaluation, args);
    case 1:
	return InvokeWithExpandedArgs<1>::invoke(func, this, env, tags,
						 needs_evaluation, args);
    ...
*/
#define CASE_STATEMENT(Z, N, IGNORED)              \
    case N:                                        \
	return InvokeWithExpandedArgs<N>::invoke(func, this, env, tags,       \
						 needs_evaluation, args);

	BOOST_PP_REPEAT_FROM_TO(0, 20, CASE_STATEMENT, 0);
#undef CASE_STATEMENT

    default:
	Rf_errorcall(const_cast<Expression*>(this),
		  _("too many arguments, sorry"));
    }
}

void Expression::matchArgsIntoEnvironment(const Closure* func,
					  Environment* calling_env,
					  const ArgList& arglist,
					  Environment* execution_env) const
{
    const ArgMatcher* matcher = func->matcher();
    ClosureContext context(this, calling_env, func, execution_env);
    matcher->match(execution_env, arglist);
}

RObject* Expression::invokeClosure(const Closure* func,
                                       Environment* calling_env,
                                       const ArgList& parglist,
                                       const Frame* method_bindings) const
{
    // We can't modify parglist, as it's on the other side of a
    // GCStackFrameboundary, so make a copy instead.
    ArgList arglist(parglist);
    arglist.wrapInPromises(calling_env, this);

    Environment* execution_env = func->createExecutionEnv(arglist);
    matchArgsIntoEnvironment(func, calling_env, arglist, execution_env);

    // If this is a method call, merge in supplementary bindings and modify
    // calling_env.
    if (method_bindings) {
      importMethodBindings(method_bindings, execution_env->frame());
      calling_env = getMethodCallingEnv();
    }

    RObject* result;
    {
      // Evaluate the function.
      ClosureContext context(this, calling_env, func, execution_env);
      result = func->execute(execution_env);
    }

    Environment::monitorLeaks(result);
    execution_env->maybeDetachFrame();

    return result;
}

void Expression::importMethodBindings(const Frame* method_bindings,
                                      Frame* newframe)
{
  method_bindings->visitBindings([&](const Frame::Binding* binding) {
      const Symbol* sym = binding->symbol();
      if (!newframe->binding(sym)) {
        newframe->importBinding(binding);
      }
    });
}

Environment* Expression::getMethodCallingEnv() {
  FunctionContext* fctxt = FunctionContext::innermost();
  while (fctxt && fctxt->function()->sexptype() == SPECIALSXP)
      fctxt = FunctionContext::innermost(fctxt->nextOut());
  return (fctxt ? fctxt->callEnvironment() : Environment::global());
}

const char* Expression::typeName() const
{
    return staticTypeName();
}

CachingExpression* CachingExpression::clone() const
{
    return new CachingExpression(*this);
}

void CachingExpression::visitReferents(const_visitor* v) const
{
    Expression::visitReferents(v);
    if (m_cached_matching_info)
        (*v)(m_cached_matching_info);
}

void CachingExpression::detachReferents()
{
    m_cached_matching_info = nullptr;
    Expression::detachReferents();
}

void CachingExpression::matchArgsIntoEnvironment(const Closure* func,
                                          Environment* calling_env,
                                          const ArgList& arglist,
                                          Environment* execution_env) const
{
    const ArgMatcher* matcher = func->matcher();
    matcher->match(execution_env, arglist, &m_cached_matching_info);
}

// ***** C interface *****

SEXP Rf_currentExpression()
{
    return R_CurrentExpr;
}

SEXP Rf_lcons(SEXP cr, SEXP tl)
{
    GCStackRoot<> crr(cr);
    GCStackRoot<PairList> tlr(SEXP_downcast<PairList*>(tl));
    return new CachingExpression(crr, tlr);
}

void Rf_setCurrentExpression(SEXP e)
{
    R_CurrentExpr = e;
}
