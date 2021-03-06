/*******************************************************************\

Module: C++ Language Type Checking

Author: Daniel Kroening, kroening@cs.cmu.edu

\*******************************************************************/

#include <util/expr_util.h>
#include <util/source_location.h>

#include "cpp_typecheck.h"
#include "cpp_convert_type.h"
#include "cpp_declarator_converter.h"
#include "cpp_template_type.h"
#include "cpp_util.h"
#include "cpp_exception_id.h"

/*******************************************************************\

Function: cpp_typecheckt::typecheck_code

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_code(codet &code)
{
  const irep_idt &statement=code.get_statement();

  if(statement==ID_try_catch)
  {
    code.type()=code_typet();
    typecheck_try_catch(code);
  }
  else if(statement==ID_member_initializer)
  {
    code.type()=code_typet();
    typecheck_member_initializer(code);
  }
  else if(statement==ID_msc_if_exists ||
          statement==ID_msc_if_not_exists)
  {
  }
  else
    c_typecheck_baset::typecheck_code(code);
}

/*******************************************************************\

Function: cpp_typecheckt::typecheck_try_catch

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_try_catch(codet &code)
{
  codet::operandst &operands=code.operands();

  for(codet::operandst::iterator
      it=operands.begin();
      it!=operands.end();
      it++)
  {
    if(it==operands.begin())
    {
      // this is the 'try'
      typecheck_code(to_code(*it));
    }
    else
    {
      // This is (one of) the catch clauses.
      codet &code=to_code_block(to_code(*it));

      // look at the catch operand
      assert(!code.operands().empty());

      if(to_code(code.op0()).get_statement()==ID_ellipsis)
      {
        code.operands().erase(code.operands().begin());

        // do body
        typecheck_code(code);
      }
      else
      {
        // turn references into non-references
        {
          assert(to_code(code.op0()).get_statement()==ID_decl);
          cpp_declarationt &cpp_declaration=
            to_cpp_declaration(to_code_decl(to_code(code.op0())).symbol());

          assert(cpp_declaration.declarators().size()==1);
          cpp_declaratort &declarator=cpp_declaration.declarators().front();

          if(is_reference(declarator.type()))
            declarator.type()=declarator.type().subtype();
        }

        // typecheck the body
        typecheck_code(code);

        // the declaration is now in a decl_block

        assert(!code.operands().empty());
        assert(to_code(code.op0()).get_statement()==ID_decl_block);

        // get the declaration
        const code_declt &code_decl=
          to_code_decl(to_code(code.op0().op0()));

        // get the type
        const typet &type=code_decl.op0().type();

        // annotate exception ID
        it->set(ID_exception_id, cpp_exception_id(type, *this));
      }
    }
  }
}

/*******************************************************************\

Function: cpp_typecheckt::typecheck_ifthenelse

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_ifthenelse(code_ifthenelset &code)
{
  // In addition to the C syntax, C++ also allows a declaration
  // as condition. E.g.,
  // if(void *p=...) ...

  if(code.cond().id()==ID_code)
  {
    typecheck_code(to_code(code.cond()));
  }
  else
    c_typecheck_baset::typecheck_ifthenelse(code);
}

/*******************************************************************\

Function: cpp_typecheckt::typecheck_while

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_while(code_whilet &code)
{
  // In addition to the C syntax, C++ also allows a declaration
  // as condition. E.g.,
  // while(void *p=...) ...

  if(code.cond().id()==ID_code)
  {
    typecheck_code(to_code(code.cond()));
  }
  else
    c_typecheck_baset::typecheck_while(code);
}

/*******************************************************************\

Function: cpp_typecheckt::typecheck_switch

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_switch(code_switcht &code)
{
  // In addition to the C syntax, C++ also allows a declaration
  // as condition. E.g.,
  // switch(int i=...) ...

  if(code.value().id()==ID_code)
  {
    // we shall rewrite that into
    // { int i=....; switch(i) .... }

    codet decl=to_code(code.value());
    typecheck_decl(decl);

    assert(decl.get_statement()==ID_decl_block);
    assert(decl.operands().size()==1);

    // replace declaration by its symbol
    assert(decl.op0().op0().id()==ID_symbol);
    code.value()=decl.op0().op0();

    c_typecheck_baset::typecheck_switch(code);

    code_blockt code_block;
    code_block.move_to_operands(decl.op0(), code);
    code.swap(code_block);
  }
  else
    c_typecheck_baset::typecheck_switch(code);
}

/*******************************************************************\

Function: cpp_typecheckt::typecheck_member_initializer

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_member_initializer(codet &code)
{
  const cpp_namet &member=
    to_cpp_name(code.find(ID_member));

  // Let's first typecheck the operands.
  Forall_operands(it, code)
    typecheck_expr(*it);

  // The initializer may be a data member (non-type)
  // or a parent class (type).
  // We ask for VAR only, as we get the parent classes via their
  // constructor!
  cpp_typecheck_fargst fargs;
  fargs.in_use=true;
  fargs.operands=code.operands();

  // We should only really resolve in qualified mode,
  // no need to look into the parent.
  // Plus, this should happen in class scope, not the scope of
  // the constructor because of the constructor arguments.
  exprt symbol_expr=
    resolve(member, cpp_typecheck_resolvet::VAR, fargs);

  if(symbol_expr.type().id()==ID_code)
  {
    const code_typet &code_type=to_code_type(symbol_expr.type());

    assert(code_type.parameters().size()>=1);

    // It's a parent. Call the constructor that we got.
    side_effect_expr_function_callt function_call;

    function_call.function()=symbol_expr;
    function_call.add_source_location()=code.source_location();
    function_call.arguments().reserve(code.operands().size()+1);

    // we have to add 'this'
    exprt this_expr = cpp_scopes.current_scope().this_expr;
    assert(this_expr.is_not_nil());

    make_ptr_typecast(
      this_expr,
      code_type.parameters().front().type());

    function_call.arguments().push_back(this_expr);

    forall_operands(it, code)
      function_call.arguments().push_back(*it);

    // done building the expression, check the argument types
    typecheck_function_call_arguments(function_call);

    if(symbol_expr.get_bool("#not_accessible"))
    {
      irep_idt access = symbol_expr.get(ID_C_access);

      assert(access==ID_private ||
             access==ID_protected ||
             access=="noaccess");

      if(access==ID_private || access=="noaccess")
      {
        #if 0
        error().source_location=code.source_location());
        str << "error: constructor of `"
            << to_string(symbol_expr)
            << "' is not accessible";
        throw 0;
        #endif
      }
    }

    code_expressiont code_expression;
    code_expression.expression()=function_call;

    code.swap(code_expression);
  }
  else
  {
    // a reference member
    if(symbol_expr.id() == ID_dereference &&
       symbol_expr.op0().id() == ID_member &&
       symbol_expr.get_bool(ID_C_implicit) == true)
    {
      // treat references as normal pointers
      exprt tmp = symbol_expr.op0();
      symbol_expr.swap(tmp);
    }

    if(symbol_expr.id() == ID_symbol &&
       symbol_expr.type().id()!=ID_code)
    {
      // maybe the name of the member collides with a parameter of the constructor
      symbol_expr.make_nil();
      cpp_typecheck_fargst fargs;
      exprt dereference(ID_dereference, cpp_scopes.current_scope().this_expr.type().subtype());
      dereference.copy_to_operands(cpp_scopes.current_scope().this_expr);
      fargs.add_object(dereference);

      {
        cpp_save_scopet cpp_saved_scope(cpp_scopes);
        cpp_scopes.go_to(*(cpp_scopes.id_map[cpp_scopes.current_scope().class_identifier]));
        symbol_expr=resolve(member, cpp_typecheck_resolvet::VAR, fargs);
      }

      if(symbol_expr.id() == ID_dereference &&
         symbol_expr.op0().id() == ID_member &&
         symbol_expr.get_bool(ID_C_implicit) == true)
      {
        // treat references as normal pointers
        exprt tmp = symbol_expr.op0();
        symbol_expr.swap(tmp);
      }
    }

    if(symbol_expr.id() == ID_member &&
       symbol_expr.op0().id() == ID_dereference &&
       symbol_expr.op0().op0() == cpp_scopes.current_scope().this_expr)
    {
      if(is_reference(symbol_expr.type()))
      {
        // it's a reference member
        if(code.operands().size()!= 1)
        {
          error().source_location=code.find_source_location();
          error() << " reference `" << to_string(symbol_expr)
                  << "' expects one initializer" << eom;
          throw 0;
        }

        reference_initializer(code.op0(), symbol_expr.type());

        // assign the pointers
        symbol_expr.type().remove("#reference");
        symbol_expr.set("#lvalue", true);
        code.op0().type().remove("#reference");

        side_effect_exprt assign(ID_assign);
        assign.add_source_location() = code.source_location();
        assign.copy_to_operands(symbol_expr, code.op0());
        typecheck_side_effect_assignment(assign);
        code_expressiont new_code;
        new_code.expression()=assign;
        code.swap(new_code);
      }
      else
      {
        // it's a data member
        already_typechecked(symbol_expr);

        Forall_operands(it, code)
          already_typechecked(*it);

        exprt call=
          cpp_constructor(code.source_location(), symbol_expr, code.operands());

        if(call.is_nil())
        {
          call=codet(ID_skip);
          call.add_source_location()=code.source_location();
        }

        code.swap(call);
      }
    }
    else
    {
      error().source_location=code.find_source_location();
      error() << "invalid member initializer `"
              << to_string(symbol_expr) << "'" << eom;
      throw 0;
    }
  }
}

/*******************************************************************\

Function: cpp_typecheckt::typecheck_decl

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_decl(codet &code)
{
  if(code.operands().size()!=1)
  {
    error().source_location=code.find_source_location();
    error() << "declaration expected to have one operand" << eom;
    throw 0;
  }

  assert(code.op0().id()==ID_cpp_declaration);

  cpp_declarationt &declaration=
    to_cpp_declaration(code.op0());

  typet &type=declaration.type();

  bool is_typedef=declaration.is_typedef();

  typecheck_type(type);
  assert(type.is_not_nil());

  if(declaration.declarators().empty() &&
     follow(type).get_bool(ID_C_is_anonymous))
  {
    if(follow(type).id()!=ID_union)
    {
      error().source_location=code.find_source_location();
      error() << "declaration statement does not declare anything"
              << eom;
      throw 0;
    }

    convert_anonymous_union(declaration, code);
    return;
  }

  codet new_code(ID_decl_block);
  new_code.reserve_operands(declaration.declarators().size());

  // Do the declarators (if any)
  for(auto &declarator : declaration.declarators())
  {
    cpp_declarator_convertert cpp_declarator_converter(*this);
    cpp_declarator_converter.is_typedef=is_typedef;

    const symbolt &symbol=
      cpp_declarator_converter.convert(declaration, declarator);

    if(is_typedef) continue;

    codet decl_statement(ID_decl);
    decl_statement.reserve_operands(2);
    decl_statement.add_source_location()=symbol.location;
    decl_statement.copy_to_operands(cpp_symbol_expr(symbol));

    // Do we have an initializer that's not code?
    if(symbol.value.is_not_nil() &&
       symbol.value.id()!=ID_code)
    {
      decl_statement.copy_to_operands(symbol.value);
      assert(follow(decl_statement.op1().type())==follow(symbol.type));
    }

    new_code.move_to_operands(decl_statement);

    // is there a constructor to be called?
    if(symbol.value.is_not_nil())
    {
      assert(declarator.find("init_args").is_nil());
      if(symbol.value.id()==ID_code)
        new_code.copy_to_operands(symbol.value);
    }
    else
    {
      exprt object_expr=cpp_symbol_expr(symbol);

      already_typechecked(object_expr);

      exprt constructor_call=
        cpp_constructor(
          symbol.location,
          object_expr,
          declarator.init_args().operands());

      if(constructor_call.is_not_nil())
        new_code.move_to_operands(constructor_call);
    }
  }

  code.swap(new_code);
}

/*******************************************************************\

Function: cpp_typecheck_codet::typecheck_block

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_block(codet &code)
{
  cpp_save_scopet saved_scope(cpp_scopes);
  cpp_scopes.new_block_scope();

  c_typecheck_baset::typecheck_block(code);
}

/*******************************************************************\

Function: cpp_typecheckt::typecheck_assign

  Inputs:

 Outputs:

 Purpose:

\*******************************************************************/

void cpp_typecheckt::typecheck_assign(codet &code)
{
  if(code.operands().size()!=2)
  {
    error().source_location=code.find_source_location();
    error() << "assignment statement expected to have two operands"
            << eom;
    throw 0;
  }

  // turn into a sideeffect
  side_effect_exprt expr(code.get(ID_statement));
  expr.operands() = code.operands();
  typecheck_expr(expr);

  code_expressiont code_expr;
  code_expr.expression()=expr;
  code_expr.add_source_location() = code.source_location();

  code.swap(code_expr);
}
