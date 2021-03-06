/*******************************************************************\

Module: Variables whose address is taken

Author: Daniel Kroening

Date: March 2013

\*******************************************************************/

#ifndef CPROVER_ANALYSES_DIRTY_H
#define CPROVER_ANALYSES_DIRTY_H

#include <unordered_set>

#include <util/std_expr.h>
#include <goto-programs/goto_functions.h>

class dirtyt
{
public:
  typedef std::unordered_set<irep_idt, irep_id_hash> id_sett;
  typedef goto_functionst::goto_functiont goto_functiont;

  explicit dirtyt(const goto_functiont &goto_function)
  {
    build(goto_function);
  }

  explicit dirtyt(const goto_functionst &goto_functions)
  {
    forall_goto_functions(it, goto_functions)
      build(it->second);
  }

  void output(std::ostream &out) const;

  inline bool operator()(const irep_idt &id) const
  {
    return dirty.find(id)!=dirty.end();
  }

  inline bool operator()(const symbol_exprt &expr) const
  {
    return operator()(expr.get_identifier());
  }

  inline const id_sett& get_dirty_ids() const
  {
    return dirty;
  }

protected:
  void build(const goto_functiont &goto_function);

  // variables whose address is taken
  id_sett dirty;

  void find_dirty(const exprt &expr);
  void find_dirty_address_of(const exprt &expr);
};

static inline std::ostream &operator << (
  std::ostream &out, const dirtyt &dirty)
{
  dirty.output(out);
  return out;
}

#endif // CPROVER_ANALYSES_DIRTY_H
