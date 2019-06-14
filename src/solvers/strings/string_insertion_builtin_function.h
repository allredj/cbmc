/*******************************************************************\

Module: Builtin functions for string insertions

Author: Romain Brenguier, Joel Allred

\*******************************************************************/

/// \file
/// Builtin functions for string insertions

#ifndef CPROVER_SOLVERS_STRINGS_STRING_INSERTION_BUILTIN_FUNCTION_H
#define CPROVER_SOLVERS_STRINGS_STRING_INSERTION_BUILTIN_FUNCTION_H

#include "string_builtin_function.h"

/// String inserting a string into another one
class string_insertion_builtin_functiont : public string_builtin_functiont
{
public:
  array_string_exprt result;
  array_string_exprt input1;
  array_string_exprt input2;
  std::vector<exprt> args;

  /// Constructor from arguments of a function application.
  /// The arguments in `fun_args` should be in order:
  /// an integer `result.length`, a character pointer `&result[0]`,
  /// a string `arg1` of type refined_string_typet,
  /// a string `arg2` of type refined_string_typet,
  /// and potentially some arguments of primitive types.
  string_insertion_builtin_functiont(
    const exprt &return_code,
    const std::vector<exprt> &fun_args,
    array_poolt &array_pool);

  optionalt<array_string_exprt> string_result() const override
  {
    return result;
  }
  std::vector<array_string_exprt> string_arguments() const override
  {
    return {input1, input2};
  }

  /// Evaluate the result from a concrete valuation of the arguments
  virtual std::vector<mp_integer> eval(
    const std::vector<mp_integer> &input1_value,
    const std::vector<mp_integer> &input2_value,
    const std::vector<mp_integer> &args_value) const;

  optionalt<exprt>
  eval(const std::function<exprt(const exprt &)> &get_value) const override;

  std::string name() const override
  {
    return "insert";
  }

  string_constraintst
  constraints(string_constraint_generatort &generator) const override;

  exprt length_constraint() const override;

  bool maybe_testing_function() const override
  {
    return false;
  }

protected:
  explicit string_insertion_builtin_functiont(
    const exprt &return_code,
    array_poolt &array_pool)
    : string_builtin_functiont(return_code, array_pool)
  {
  }
};

#endif // CPROVER_SOLVERS_STRINGS_STRING_INSERTION_BUILTIN_FUNCTION_H
