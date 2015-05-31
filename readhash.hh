// blah

#ifndef NPCH_READHASH_HH
#define NPCH_READHASH_HH

#include "gcc-plugin.h"
#include "system.h"
#include "coretypes.h"
#include "tree.h"
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include "c-tree.h"

class pointer_iterator;

class mapped_hash
{
public:

  mapped_hash (const uint8_t *data, size_t length);
  ~mapped_hash ();

  // Find a binding for NAME and KIND in this map.  If none is found,
  // return NULL_TREE.
  tree find (c_oracle_request kind, const char *name);

  // GC mark.
  void mark ();

  bool init ();

private:

  tree read_index_get_type (pointer_iterator &iter);
  tree read_int_type (pointer_iterator &iter);
  tree read_float_type (pointer_iterator &iter);
  tree read_pointer_type (pointer_iterator &iter);
  tree read_function_type (pointer_iterator &iter);
  tree read_array_type (pointer_iterator &iter);
  tree read_qualified_type (pointer_iterator &iter);
  tree read_enum_type (pointer_iterator &iter);
  tree read_bitfield_type (pointer_iterator &iter);
  tree read_struct_or_union_type (pointer_iterator &, int, bool);
  tree read_symbol (pointer_iterator &iter);
  tree read_basic (pointer_iterator &iter, int idx);
  tree find_type (size_t idx);

  // The underlying data.  FIXME maybe a better ..
  const uint8_t *m_data;
  size_t m_length;

  size_t cpool_offset;

  tree *trees;
  size_t n_trees;

  struct hasher
  {
    std::size_t operator () (const char *a) const
    {
      return htab_hash_string (a);
    }
  };

  struct equal
  {
    bool operator () (const char *a, const char *b) const
    {
      return strcmp (a, b) == 0;
    }
  };

  typedef std::unordered_map<const char *, size_t, hasher, equal> hash_map;

  hash_map symbols;
  hash_map tags;
};

#endif // NPCH_READHASH_HH
