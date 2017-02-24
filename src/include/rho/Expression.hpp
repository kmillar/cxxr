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

/** @file Expression.h
 * @brief Class rho::Expression and associated C interface.
 */

#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "rho/ArgList.hpp"
#include "rho/ArgMatcher.hpp"
#include "rho/FunctionBase.hpp"
#include "rho/PairList.hpp"

namespace rho {
    class ArgList;
    class BuiltInFunction;
    class Closure;
    class Frame;
    class FunctionBase;

    /** @brief Singly linked list representing an R expression.
     *
     * R expression, represented as a LISP-like singly-linked list,
     * each element containing pointers to a 'car' object and to a
     * 'tag' object, as well as a pointer to the next element of the
     * list.  (Any of these pointers may be null.)  A Expression
     * object is considered to 'own' its car, its tag, and all its
     * successors.
     *
     * Most expressions should be represented using the CachingExpression class,
     * instead of this, as it has better performance.  This class is primarily
     * useful for expressions that are only evaluated once, where the function
     * is known to be a primitive and for SET_TYPEOF.
     */
    class Expression : public ConsCell {
    public:
	/**
	 * @param cr Pointer to the 'car' of the element to be
	 *           constructed.
	 *
	 * @param tl Pointer to the 'tail' (LISP cdr) of the element
	 *           to be constructed.
	 *
	 * @param tg Pointer to the 'tag' of the element to be constructed.
	 */
	explicit Expression(RObject* cr = nullptr, PairList* tl = nullptr,
			    const RObject* tg = nullptr)
	    : ConsCell(LANGSXP, cr, tl, tg)
	{}

	Expression(RObject* function,
		   std::initializer_list<RObject*> unnamed_args);

	Expression(RObject* function,
		   const ArgList& arglist)
	    : Expression(function, const_cast<PairList*>(arglist.list())) {}

	/** @brief Copy constructor.
	 *
	 * @param pattern Expression to be copied.
	 */
	Expression(const Expression& pattern) = default;

        const RObject* getFunction() const {
            return car();
        }

        const PairList* getArgs() const {
            return tail();
        }

	void check1arg(const char* formal) const;

	/** @brief The name by which this type is known in R.
	 *
	 * @return the name by which this type is known in R.
	 */
	static const char* staticTypeName()
	{
	    return "language";
	}

	/** @brief Invoke the function.
	 *
	 * @param func The function to call.
	 *
	 * @param env The environment to call the function from.
	 *
	 * @param arglist The arguments to pass to the function.
	 *
	 * @param method_bindings This pointer will be non-null if and
	 *          only if this invocation represents a method call,
	 *          in which case it points to a Frame containing
	 *          Bindings that should be added to the working
	 *          environment, for example bindings of the Symbols
	 *          \c .Generic and \c .Class.
	 *
	 * @return The result of evaluating the function call.
	 */
	RObject* evaluateFunctionCall(const FunctionBase* func,
				      Environment* env,
                               const ArgList& arglist,
				      const Frame* method_bindings = nullptr)
	    const;

	// Virtual functions of RObject:
	Expression* clone() const override;
        RObject* evaluate(Environment* env) override;
	const char* typeName() const override;

    protected:
	// Declared protected to ensure that Expression objects are
	// allocated only using 'new':
	~Expression() {}

        FunctionBase* getFunction(Environment* env) const;


	RObject* evaluateFunctionCall(const FunctionBase* func,
				      Environment* env,
				      const PairList* args,
				      ArgList::Status status) const;

	RObject* evaluateBuiltInCall(const BuiltInFunction* builtin,
				     Environment* env,
				     const PairList* args,
				     ArgList::Status status) const;

	RObject* invokeNativeCallBuiltIn(const BuiltInFunction* builtin,
			      Environment* env,
					 const PairList* args, int num_args,
					 bool needs_evaluation) const;

	RObject* invokeClosure(const Closure* func,
                                   Environment* calling_env,
                                   const ArgList& arglist,
			       const Frame* method_bindings = nullptr) const;

        static void importMethodBindings(const Frame* method_bindings,
                                         Frame* newframe);
        static Environment* getMethodCallingEnv();

	virtual void matchArgsIntoEnvironment(const Closure* func,
					      Environment* calling_env,
					      const ArgList& arglist,
					      Environment* execution_env) const;

      private:
	Expression& operator=(const Expression&) = delete;
    };

    /** @brief Singly linked list representing an R expression.
     *
     * R expression, represented as a LISP-like singly-linked list,
     * each element containing pointers to a 'car' object and to a
     * 'tag' object, as well as a pointer to the next element of the
     * list.  (Any of these pointers may be null.)  A Expression
     * object is considered to 'own' its car, its tag, and all its
     * successors.
     *
     * Unlike the regular Expression class, this class caches the results of
     * parameter matching to closure calls for improved performance.
     */
    class CachingExpression : public Expression {
    public:
	/**
	 * @param cr Pointer to the 'car' of the element to be
	 *           constructed.
	 *
	 * @param tl Pointer to the 'tail' (LISP cdr) of the element
	 *           to be constructed.
	 *
	 * @param tg Pointer to the 'tag' of the element to be constructed.
	 */
	explicit CachingExpression(RObject* cr = nullptr,
				   PairList* tl = nullptr,
				   const RObject* tg = nullptr)
	    : Expression(cr, tl, tg)
	{}

	CachingExpression(RObject* function,
			  std::initializer_list<RObject*> unnamed_args)
	    : Expression(function, unnamed_args)
	{}

	/** @brief Copy constructor.
	 *
	 * @param pattern CachingExpression to be copied.
	 */
	CachingExpression(const CachingExpression& pattern)
	    : Expression(pattern) {
	    // Don't copy the cache, as the new expression may be about to get
	    // modified.
	    // TODO: is there a way we can automatically detect modifications
	    //   of *this and invalidate the cache?
	}

	CachingExpression(const Expression& pattern) : Expression(pattern)
	{}

	// Virtual functions of RObject:
	CachingExpression* clone() const override;

	// For GCNode
	void visitReferents(const_visitor* v) const override;
    protected:
	void detachReferents() override;
    private:
	// Object used for recording details from previous evaluations of
	// this expression, for the purpose of optimizing future evaluations.
	// In the future, this will likely include type recording as well.
        mutable GCEdge<const ArgMatchCache> m_cached_matching_info;

	void matchArgsIntoEnvironment(const Closure* func,
				      Environment* calling_env,
				      const ArgList& arglist,
				      Environment* execution_env)
	    const override;

	// Declared private to ensure that CachingExpression objects are
	// allocated only using 'new':
	~CachingExpression() {}

	CachingExpression& operator=(const CachingExpression&) = delete;
    };

} // namespace rho

/** @brief Pointer to expression currently being evaluated.
 */
extern rho::GCRoot<> R_CurrentExpr;

/** @brief Expression currently being evaluated.
 *
 * @return Pointer to the Expression currently being evaluated.
 */
SEXP Rf_currentExpression();

/** @brief Designate the Expression currently being evaluated.
 *
 * @param e Pointer to the Expression now to be evaluated.  (Not
 *          currently checked in any way.)
 */
void Rf_setCurrentExpression(SEXP e);

#endif /* EXPRESSION_H */
