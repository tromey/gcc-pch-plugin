// Read one of our hash tables.

#include "readhash.hh"
#include <assert.h>
#include <cstdint>
#include <cstdlib>
#include <string.h>
#include <memory>
#include "stringpool.h"
#include "tree.h"
#include "version.hh"

class pointer_iterator
{
public:

  pointer_iterator (const uint8_t *data, size_t length)
    : m_p (data), m_data (data), m_end (data + length)
  {
  }

  uint8_t operator* () const
  {
    assert (m_p >= m_data && m_p < m_end);
    return *m_p;
  }

  bool advance (size_t delta = 1)
  {
    m_p += delta;
    return m_p >= m_data && m_p < m_end;
  }

  const char *read_string ()
  {
    assert (m_p >= m_data && m_p < m_end);
    const char *result = (const char *) m_p;
    // FIXME could overflow here.
    m_p += strlen (result) + 1;
    if (m_p < m_data || m_p >= m_end)
      return nullptr;
    return result;
  }

  bool read_int (int *result)
  {
    if (m_p + 3 >= m_end)
      return false;
    *result = 0;
    for (int i = 3; i >= 0; --i)
      {
	*result <<= 8;
	*result |= m_p[i] & 0xff;
      }
    m_p += 4;
    return true;
  }

  char read_char ()
  {
    if (m_p >= m_end)
      return 0;
    return *m_p++;
  }

  size_t get_offset () const
  {
    return m_p - m_data;
  }

private:

  const uint8_t *m_p;
  const uint8_t *m_data;
  const uint8_t *m_end;
};

mapped_hash::mapped_hash (const uint8_t *data, size_t length)
  : m_data (data),
    m_length (length),
    trees (nullptr)
{
}

mapped_hash::~mapped_hash ()
{
  delete[] trees;
}

bool
mapped_hash::init ()
{
  pointer_iterator iter (m_data, m_length);

  int version;
  if (!iter.read_int (&version) || version != PCH_PLUGIN_VERSION)
    {
      fprintf (stderr, "[version fail]\n");
      return false;
    }

  for (int i = 0; i < 2; ++i)
    {
      hash_map &the_map = (i == 0) ? symbols : tags;
      while (true)
	{
	  const char *str = iter.read_string ();
	  if (*str == '\0')
	    break;

	  int offset;
	  if (!iter.read_int (&offset))
	    return false;

	  the_map[str] = offset;
	}
    }

  cpool_offset = iter.get_offset ();
  n_trees = m_length - cpool_offset + 1;
  // Memory overkill.
  trees = new tree[n_trees];
  memset (trees, 0, n_trees * sizeof (tree));
  return true;
}

tree
mapped_hash::find (c_oracle_request kind, const char *name)
{
  if (kind != C_ORACLE_SYMBOL && kind != C_ORACLE_TAG)
    return NULL_TREE;

  hash_map &lookup = (kind == C_ORACLE_TAG) ? tags : symbols;
  auto iter = lookup.find (name);
  if (iter == lookup.end ())
    return NULL_TREE;
  return find_type ((*iter).second);
}

void
mapped_hash::mark ()
{
  for (size_t i = 0; i < n_trees; ++i)
    ggc_mark (trees[i]);
}

tree
mapped_hash::read_index_get_type (pointer_iterator &iter)
{
  int i;
  if (!iter.read_int (&i))
    return error_mark_node;
  return find_type (i);
}

tree
mapped_hash::read_int_type (pointer_iterator &iter)
{
  int size_in_bytes;
  if (!iter.read_int (&size_in_bytes))
    return error_mark_node;
  bool is_unsigned = size_in_bytes > 0;
  if (size_in_bytes < 0)
    size_in_bytes = - size_in_bytes;
  return c_common_type_for_size (BITS_PER_UNIT * size_in_bytes, is_unsigned);
}

tree
mapped_hash::read_float_type (pointer_iterator &iter)
{
  int size_in_bytes;
  if (!iter.read_int (&size_in_bytes))
    return error_mark_node;
  if (BITS_PER_UNIT * size_in_bytes == TYPE_PRECISION (float_type_node))
    return float_type_node;
  if (BITS_PER_UNIT * size_in_bytes == TYPE_PRECISION (double_type_node))
    return double_type_node;
  if (BITS_PER_UNIT * size_in_bytes == TYPE_PRECISION (long_double_type_node))
    return long_double_type_node;
  return error_mark_node;
}

tree
mapped_hash::read_pointer_type (pointer_iterator &iter)
{
  tree base_type = read_index_get_type (iter);
  if (base_type == error_mark_node)
    return error_mark_node;
  return build_pointer_type (base_type);
}

tree
mapped_hash::read_function_type (pointer_iterator &iter)
{
  int num_args, is_varargs;
  if (!iter.read_int (&num_args) || !iter.read_int (&is_varargs))
    return error_mark_node;
  tree return_type = read_index_get_type (iter);
  if (return_type == error_mark_node)
    return error_mark_node;

  std::unique_ptr<tree[]> argument_types (new tree[num_args]);
  for (int i = 0; i < num_args; ++i)
    {
      argument_types[i] = read_index_get_type (iter);
      if (argument_types[i] == error_mark_node)
	return error_mark_node;
    }

  tree result;
  if (is_varargs)
    result = build_varargs_function_type_array (return_type, num_args,
						argument_types.get ());
  else
    result = build_function_type_array (return_type, num_args,
					argument_types.get ());

  return result;
}

tree
mapped_hash::read_array_type (pointer_iterator &iter)
{
  int num_elements;
  if (!iter.read_int (&num_elements))
    return error_mark_node;
  tree element_type = read_index_get_type (iter);
  if (element_type == error_mark_node)
    return error_mark_node;
  if (num_elements == -1)
    return build_array_type (element_type, NULL_TREE);
  else
    return build_array_type_nelts (element_type, num_elements);
}

tree
mapped_hash::read_qualified_type (pointer_iterator &iter)
{
  int quals;
  if (!iter.read_int (&quals))
    return error_mark_node;
  tree base_type = read_index_get_type (iter);
  if (base_type == error_mark_node)
    return error_mark_node;
  return build_qualified_type (base_type, quals);
}

tree
mapped_hash::read_enum_type (pointer_iterator &iter)
{
  int num_elements;
  if (!iter.read_int (&num_elements))
    return error_mark_node;
  tree result = make_node (ENUMERAL_TYPE);
  // underlying type?
  for (int i = 0; i < num_elements; ++i)
    {
      const char *name = iter.read_string ();
      if (name == nullptr)
	return error_mark_node;
      unsigned long value;
#if fixme
      if (!iter.read_ulong (&value))
#endif
	return error_mark_node;
      tree cst = build_int_cst (result, value);
      tree decl = build_decl (BUILTINS_LOCATION /* FIXME */,
			      CONST_DECL, get_identifier (name), result);
      DECL_INITIAL (decl) = cst;
      // pushdecl_safe (decl);
      tree cons = tree_cons (DECL_NAME (decl), cst, TYPE_VALUES (result));
      TYPE_VALUES (result) = cons;
    }
  return result;
}

tree
mapped_hash::read_bitfield_type (pointer_iterator &iter)
{
  int bitsize;
  if (!iter.read_int (&bitsize))
    return error_mark_node;
#if fixme
  return c_build_bitfield_integer_type (bitsize, fixme);
#else
  return error_mark_node;
#endif
}

tree
mapped_hash::read_struct_or_union_type (pointer_iterator &iter, int type_index,
					bool is_struct)
{
  int num_fields;
  if (!iter.read_int (&num_fields))
    return error_mark_node;

  assert (trees[type_index] == NULL_TREE);
  trees[type_index] = make_node (is_struct ? RECORD_TYPE : UNION_TYPE);
  for (int i = 0; i < num_fields; ++i)
    {
      const char *field_name = iter.read_string ();
      if (field_name == nullptr)
	return error_mark_node;
      tree field_type = read_index_get_type (iter);
      if (field_type == error_mark_node)
	return error_mark_node;

      tree name = *field_name == '\0' ? NULL_TREE : get_identifier (field_name);
      tree decl = build_decl (BUILTINS_LOCATION /* FIXME */, FIELD_DECL,
			      name, field_type);
      DECL_FIELD_CONTEXT (decl) = trees[type_index];

      DECL_CHAIN (decl) = TYPE_FIELDS (trees[type_index]);
      TYPE_FIELDS (trees[type_index]) = decl;
    }

  /* We built the field list in reverse order, so fix it now.  */
  TYPE_FIELDS (trees[type_index]) = nreverse (TYPE_FIELDS (trees[type_index]));
  return trees[type_index];
}

tree
mapped_hash::read_symbol (pointer_iterator &iter)
{
  char what = iter.read_char ();
  if (!what)
    return error_mark_node;
  const char *symname = iter.read_string ();
  if (symname == nullptr)
    return error_mark_node;
  tree type = read_index_get_type (iter);
  if (type == error_mark_node)
    return error_mark_node;

  tree_code code;
  switch (what)
    {
    case 'f':
      code = FUNCTION_DECL;
      break;
    case 'v':
      code = VAR_DECL;
      break;
    case 't':
      code = TYPE_DECL;
      break;
    default:
      return error_mark_node;
    }

  return build_decl (BUILTINS_LOCATION /* FIXME */, code,
		     get_identifier (symname), type);
}

tree
mapped_hash::read_basic (pointer_iterator &iter, int idx)
{
  char c = iter.read_char ();
  switch (c)
    {
    case 'i':
      return read_int_type (iter);
    case 'f':
      return read_float_type (iter);
    case 'p':
      return read_pointer_type (iter);
    case '(':
      return read_function_type (iter);
    case '[':
      return read_array_type (iter);
    case 'q':
      return read_qualified_type (iter);
    case 'e':
      return read_enum_type (iter);
    case ':':
      return read_bitfield_type (iter);
    case '{':
    case '|':
      return read_struct_or_union_type (iter, idx, c == '{');
    case 'S':
      return read_symbol (iter);
    case 'V':
      return void_type_node;
    }

  return error_mark_node;
}

tree
mapped_hash::find_type (size_t idx)
{
  if (!trees[idx])
    {
      pointer_iterator iter (m_data, m_length);
      iter.advance(cpool_offset + idx);
      trees[idx] = read_basic (iter, idx);
    }
  return trees[idx];
}
